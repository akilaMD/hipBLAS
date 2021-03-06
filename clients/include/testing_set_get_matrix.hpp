/* ************************************************************************
 * Copyright 2016-2020 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "cblas_interface.h"
#include "flops.h"
#include "hipblas.hpp"
#include "hipblas_fortran.hpp"
#include "norm.h"
#include "unit.h"
#include "utility.h"

using namespace std;

/* ============================================================================================ */

template <typename T>
hipblasStatus_t testing_set_get_matrix(Arguments argus)
{
    bool FORTRAN            = argus.fortran;
    auto hipblasSetMatrixFn = FORTRAN ? hipblasSetMatrixFortran : hipblasSetMatrix;
    auto hipblasGetMatrixFn = FORTRAN ? hipblasGetMatrixFortran : hipblasGetMatrix;

    int rows = argus.rows;
    int cols = argus.cols;
    int lda  = argus.lda;
    int ldb  = argus.ldb;
    int ldc  = argus.ldc;

    hipblasStatus_t status     = HIPBLAS_STATUS_SUCCESS;
    hipblasStatus_t status_set = HIPBLAS_STATUS_SUCCESS;
    hipblasStatus_t status_get = HIPBLAS_STATUS_SUCCESS;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(rows < 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }
    else if(cols < 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }
    else if(lda <= 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }
    else if(ldb <= 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }
    else if(ldc <= 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> ha(cols * lda);
    host_vector<T> hb(cols * ldb);
    host_vector<T> hb_ref(cols * ldb);
    host_vector<T> hc(cols * ldc);

    device_vector<T> dc(cols * ldc);

    double gpu_time_used, cpu_time_used;
    double hipblasBandwidth, cpu_bandwidth;
    double rocblas_error = 0.0;

    hipblasHandle_t handle;

    hipblasCreate(&handle);

    // Initial Data on CPU
    srand(1);
    hipblas_init<T>(ha, rows, cols, lda);
    hipblas_init<T>(hb, rows, cols, ldb);
    hb_ref = hb;
    for(int i = 0; i < cols * ldc; i++)
    {
        hc[i] = 100 + i;
    };
    CHECK_HIP_ERROR(hipMemcpy(dc, hc.data(), sizeof(T) * ldc * cols, hipMemcpyHostToDevice));
    for(int i = 0; i < cols * ldc; i++)
    {
        hc[i] = 99.0;
    };

    /* =====================================================================
           ROCBLAS
    =================================================================== */

    status_set = hipblasSetMatrixFn(rows, cols, sizeof(T), (void*)ha.data(), lda, (void*)dc, ldc);
    status_get = hipblasGetMatrixFn(rows, cols, sizeof(T), (void*)dc, ldc, (void*)hb.data(), ldb);
    if(status_set != HIPBLAS_STATUS_SUCCESS)
    {
        hipblasDestroy(handle);
        return status_set;
    }

    if(status_get != HIPBLAS_STATUS_SUCCESS)
    {
        hipblasDestroy(handle);
        return status_get;
    }

    if(argus.unit_check)
    {
        /* =====================================================================
           CPU BLAS
        =================================================================== */

        // reference calculation
        for(int i1 = 0; i1 < rows; i1++)
        {
            for(int i2 = 0; i2 < cols; i2++)
            {
                hb_ref[i1 + i2 * ldb] = ha[i1 + i2 * lda];
            }
        }

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(argus.unit_check)
        {
            unit_check_general<T>(rows, cols, ldb, hb.data(), hb_ref.data());
        }
    }

    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}
