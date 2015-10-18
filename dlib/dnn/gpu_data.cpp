// Copyright (C) 2015  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.
#ifndef DLIB_GPU_DaTA_CPP_
#define DLIB_GPU_DaTA_CPP_

// Only things that require CUDA are declared in this cpp file.  Everything else is in the
// gpu_data.h header so that it can operate as "header-only" code when using just the CPU.
#ifdef DLIB_USE_CUDA

#include "gpu_data.h"
#include <iostream>
#include "cuda_utils.h"


namespace dlib
{

// ----------------------------------------------------------------------------------------

    void gpu_data::
    wait_for_transfer_to_finish() const
    {
        if (have_active_transfer)
        {
            std::cout << "wait for cudaStreamSynchronize()" << std::endl;
            CHECK_CUDA(cudaStreamSynchronize((cudaStream_t)cuda_stream.get()));
            have_active_transfer = false;
            // Check for errors.  These calls to cudaGetLastError() are what help us find
            // out if our kernel launches have been failing.
            CHECK_CUDA(cudaGetLastError());
        }
    }

    void gpu_data::
    copy_to_device() const
    {
        wait_for_transfer_to_finish();
        if (!device_current)
        {
            std::cout << "cudaMemcpy to device" << std::endl;
            CHECK_CUDA(cudaMemcpy(data_device.get(), data_host.get(), data_size*sizeof(float), cudaMemcpyHostToDevice));
            device_current = true;
            // Check for errors.  These calls to cudaGetLastError() are what help us find
            // out if our kernel launches have been failing.
            CHECK_CUDA(cudaGetLastError());
        }
    }

    void gpu_data::
    copy_to_host() const
    {
        wait_for_transfer_to_finish();
        if (!host_current)
        {
            std::cout << "cudaMemcpy to host" << std::endl;
            CHECK_CUDA(cudaMemcpy(data_host.get(), data_device.get(), data_size*sizeof(float), cudaMemcpyDeviceToHost));
            host_current = true;
            // Check for errors.  These calls to cudaGetLastError() are what help us find
            // out if our kernel launches have been failing.
            CHECK_CUDA(cudaGetLastError());
        }
    }

    void gpu_data::
    async_copy_to_device() 
    {
        if (!device_current)
        {
            std::cout << "cudaMemcpyAsync to device" << std::endl;
            CHECK_CUDA(cudaMemcpyAsync(data_device.get(), data_host.get(), data_size*sizeof(float), cudaMemcpyHostToDevice, (cudaStream_t)cuda_stream.get()));
            have_active_transfer = true;
            device_current = true;
        }
    }

    void gpu_data::
    set_size(
        size_t new_size
    )
    {
        wait_for_transfer_to_finish();
        if (new_size == 0)
        {
            data_size = 0;
            host_current = true;
            device_current = true;
            data_host.reset();
            data_device.reset();
        }
        else if (new_size != data_size)
        {
            data_size = new_size;
            host_current = true;
            device_current = true;

            try
            {
                void* data;
                CHECK_CUDA(cudaMallocHost(&data, new_size*sizeof(float)));
                // Note that we don't throw exceptions since the free calls are invariably
                // called in destructors.  They also shouldn't fail anyway unless someone
                // is resetting the GPU card in the middle of their program.
                data_host.reset((float*)data, [](float* ptr){
                    auto err = cudaFreeHost(ptr);
                    if(err!=cudaSuccess)
                        std::cerr << "cudaFreeHost() failed. Reason: " << cudaGetErrorString(err) << std::endl;
                });

                CHECK_CUDA(cudaMalloc(&data, new_size*sizeof(float)));
                data_device.reset((float*)data, [](float* ptr){
                    auto err = cudaFree(ptr);
                    if(err!=cudaSuccess)
                        std::cerr << "cudaFree() failed. Reason: " << cudaGetErrorString(err) << std::endl;
                });

                if (!cuda_stream)
                {
                    cudaStream_t cstream;
                    CHECK_CUDA(cudaStreamCreateWithFlags(&cstream, cudaStreamNonBlocking));
                    cuda_stream.reset(cstream, [](void* ptr){
                        auto err = cudaStreamDestroy((cudaStream_t)ptr);
                        if(err!=cudaSuccess)
                            std::cerr << "cudaStreamDestroy() failed. Reason: " << cudaGetErrorString(err) << std::endl;
                    });
                }

            }
            catch(...)
            {
                set_size(0);
                throw;
            }
        }
    }

// ----------------------------------------------------------------------------------------
}

#endif // DLIB_USE_CUDA

#endif // DLIB_GPU_DaTA_CPP_
