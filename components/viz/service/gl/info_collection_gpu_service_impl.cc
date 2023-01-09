// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/info_collection_gpu_service_impl.h"

#include <utility>
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "gpu/config/dx_diag_node.h"
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
  NOTREACHED();
}

void InfoCollectionGpuServiceImpl::BindOnIO(
    mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  DCHECK(!receiver_.is_bound());

  receiver_.Bind(std::move(pending_receiver));
}

void InfoCollectionGpuServiceImpl::GetGpuSupportedDx12VersionAndDevicePerfInfo(
    GetGpuSupportedDx12VersionAndDevicePerfInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InfoCollectionGpuServiceImpl::
                         GetGpuSupportedDx12VersionAndDevicePerfInfoOnMain,
                     base::Unretained(this), std::move(callback)));
}

void InfoCollectionGpuServiceImpl::
    GetGpuSupportedDx12VersionAndDevicePerfInfoOnMain(
        GetGpuSupportedDx12VersionAndDevicePerfInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  uint32_t d3d12_feature_level = 0;
  uint32_t highest_shader_model_version = 0;
  gpu::GetGpuSupportedD3D12Version(d3d12_feature_level,
                                   highest_shader_model_version);
  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), d3d12_feature_level,
                     highest_shader_model_version, device_perf_info_));
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

void InfoCollectionGpuServiceImpl::RequestDxDiagNodeInfo(
    RequestDxDiagNodeInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InfoCollectionGpuServiceImpl::RequestDxDiagNodeInfoOnMain,
                     base::Unretained(this), std::move(callback)));
}

void InfoCollectionGpuServiceImpl::RequestDxDiagNodeInfoOnMain(
    RequestDxDiagNodeInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  // We can continue on shutdown here because we're not writing any critical
  // state in this task.
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce([]() {
            gpu::DxDiagNode dx_diag_node;
            gpu::GetDxDiagnostics(&dx_diag_node);
            return dx_diag_node;
          }),
          base::BindOnce(
              [](RequestDxDiagNodeInfoCallback callback,
                 scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                 const gpu::DxDiagNode& dx_diag_node) {
                io_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), dx_diag_node));
              },
              std::move(callback), io_runner_));
}

}  // namespace viz
