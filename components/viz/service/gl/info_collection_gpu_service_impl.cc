// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/info_collection_gpu_service_impl.h"

#include <utility>
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/config/gpu_info_collector.h"

namespace viz {

InfoCollectionGpuServiceImpl::InfoCollectionGpuServiceImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    const gpu::DevicePerfInfo& device_perf_info,
    const gpu::GPUInfo::GPUDevice& gpu_device,
    mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver)
    : main_runner_(std::move(main_runner)),
      io_runner_(std::move(io_runner)),
      device_perf_info_(device_perf_info),
      gpu_device_(gpu_device) {
  DCHECK(!io_runner_->BelongsToCurrentThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InfoCollectionGpuServiceImpl::BindOnIO,
                     base::Unretained(this), std::move(pending_receiver)));
}

InfoCollectionGpuServiceImpl::~InfoCollectionGpuServiceImpl() {
  // Info Collection GPU process exits atomically.
  NOTREACHED_IN_MIGRATION();
}

void InfoCollectionGpuServiceImpl::BindOnIO(
    mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  DCHECK(!receiver_.is_bound());

  receiver_.Bind(std::move(pending_receiver));
}

void InfoCollectionGpuServiceImpl::
    GetGpuSupportedDirectXVersionAndDevicePerfInfo(
        GetGpuSupportedDirectXVersionAndDevicePerfInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InfoCollectionGpuServiceImpl::
                         GetGpuSupportedDirectXVersionAndDevicePerfInfoOnMain,
                     base::Unretained(this), std::move(callback)));
}

void InfoCollectionGpuServiceImpl::
    GetGpuSupportedDirectXVersionAndDevicePerfInfoOnMain(
        GetGpuSupportedDirectXVersionAndDevicePerfInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  uint32_t d3d12_feature_level = 0;
  uint32_t highest_shader_model_version = 0;
  uint32_t directml_feature_level = 0;
  gpu::GetGpuSupportedDirectXVersion(d3d12_feature_level,
                                     highest_shader_model_version,
                                     directml_feature_level);
  io_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), d3d12_feature_level,
                                highest_shader_model_version,
                                directml_feature_level, device_perf_info_));
}

void InfoCollectionGpuServiceImpl::GetGpuSupportedVulkanVersionInfo(
    GetGpuSupportedVulkanVersionInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InfoCollectionGpuServiceImpl::GetGpuSupportedVulkanVersionInfoOnMain,
          base::Unretained(this), std::move(callback)));
}

void InfoCollectionGpuServiceImpl::GetGpuSupportedVulkanVersionInfoOnMain(
    GetGpuSupportedVulkanVersionInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  uint32_t vulkan_version = gpu::GetGpuSupportedVulkanVersion(gpu_device_);
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), vulkan_version));
}

}  // namespace viz
