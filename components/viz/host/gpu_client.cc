// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/gpu_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/host/host_gpu_memory_buffer_manager.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace viz {
namespace {
bool IsSizeValid(const gfx::Size& size) {
  base::CheckedNumeric<int> bytes = size.width();
  bytes *= size.height();
  return bytes.IsValid();
}

}  // namespace

GpuClient::GpuClient(std::unique_ptr<GpuClientDelegate> delegate,
                     int client_id,
                     uint64_t client_tracing_id,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : delegate_(std::move(delegate)),
      client_id_(client_id),
      client_tracing_id_(client_tracing_id),
      task_runner_(std::move(task_runner)) {
  DCHECK(delegate_);
  gpu_receivers_.set_disconnect_handler(
      base::BindRepeating(&GpuClient::OnError, base::Unretained(this),
                          ErrorReason::kConnectionLost));
}

GpuClient::~GpuClient() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  gpu_receivers_.Clear();
  OnError(ErrorReason::kInDestructor);
}

void GpuClient::Add(mojo::PendingReceiver<mojom::Gpu> receiver) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  gpu_receivers_.Add(this, std::move(receiver));
}

void GpuClient::OnError(ErrorReason reason) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ClearCallback();
  if (gpu_receivers_.empty() && delegate_) {
    if (auto* gpu_memory_buffer_manager =
            delegate_->GetGpuMemoryBufferManager()) {
      gpu_memory_buffer_manager->DestroyAllGpuMemoryBufferForClient(client_id_);
    }
  }
  if (reason == ErrorReason::kConnectionLost && connection_error_handler_)
    std::move(connection_error_handler_).Run(this);
}

void GpuClient::PreEstablishGpuChannel() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GpuClient::EstablishGpuChannel,
                                          weak_factory_.GetWeakPtr(),
                                          EstablishGpuChannelCallback()));
    return;
  }

  EstablishGpuChannel(EstablishGpuChannelCallback());
}

void GpuClient::SetClientPid(base::ProcessId client_pid) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuClient::SetClientPid,
                                  weak_factory_.GetWeakPtr(), client_pid));
    return;
  }

  if (GpuHostImpl* gpu_host = delegate_->EnsureGpuHost())
    gpu_host->SetChannelClientPid(client_id_, client_pid);
}

void GpuClient::SetDiskCacheHandle(const gpu::GpuDiskCacheHandle& handle) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GpuClient::SetDiskCacheHandle,
                                          weak_factory_.GetWeakPtr(), handle));
    return;
  }

  if (GpuHostImpl* gpu_host = delegate_->EnsureGpuHost())
    gpu_host->SetChannelDiskCacheHandle(client_id_, handle);
}

void GpuClient::RemoveDiskCacheHandles() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GpuClient::RemoveDiskCacheHandles,
                                          weak_factory_.GetWeakPtr()));
    return;
  }

  if (GpuHostImpl* gpu_host = delegate_->EnsureGpuHost())
    gpu_host->RemoveChannelDiskCacheHandles(client_id_);
}

void GpuClient::SetConnectionErrorHandler(
    ConnectionErrorHandlerClosure connection_error_handler) {
  connection_error_handler_ = std::move(connection_error_handler);
}

base::WeakPtr<GpuClient> GpuClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void GpuClient::OnEstablishGpuChannel(
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    GpuHostImpl::EstablishChannelStatus status) {
  DCHECK_EQ(channel_handle.is_valid(),
            status == GpuHostImpl::EstablishChannelStatus::kSuccess);
  gpu_channel_requested_ = false;
  EstablishGpuChannelCallback callback = std::move(callback_);

  if (status == GpuHostImpl::EstablishChannelStatus::kGpuHostInvalid) {
    // GPU process may have crashed or been killed. Try again.
    EstablishGpuChannel(std::move(callback));
    return;
  }
  if (callback) {
    // A request is waiting.
    std::move(callback).Run(client_id_, std::move(channel_handle), gpu_info,
                            gpu_feature_info);
    return;
  }
  if (status == GpuHostImpl::EstablishChannelStatus::kSuccess) {
    // This is the case we pre-establish a channel before a request arrives.
    // Cache the channel for a future request.
    channel_handle_ = std::move(channel_handle);
    gpu_info_ = gpu_info;
    gpu_feature_info_ = gpu_feature_info;
  }
}

void GpuClient::OnCreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                        gfx::GpuMemoryBufferHandle handle) {
  auto it = pending_create_callbacks_.find(id);
  DCHECK(it != pending_create_callbacks_.end());
  CreateGpuMemoryBufferCallback callback = std::move(it->second);
  pending_create_callbacks_.erase(it);
  std::move(callback).Run(std::move(handle));
}

void GpuClient::ClearCallback() {
  if (!callback_)
    return;
  EstablishGpuChannelCallback callback = std::move(callback_);
  std::move(callback).Run(client_id_, mojo::ScopedMessagePipeHandle(),
                          gpu::GPUInfo(), gpu::GpuFeatureInfo());
}

void GpuClient::EstablishGpuChannel(EstablishGpuChannelCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // At most one channel should be requested. So clear previous request first.
  ClearCallback();

  if (channel_handle_.is_valid()) {
    // If a channel has been pre-established and cached,
    //   1) if callback is valid, return it right away.
    //   2) if callback is empty, it's PreEstablishGpyChannel() being called
    //      more than once, no need to do anything.
    if (callback) {
      std::move(callback).Run(client_id_, std::move(channel_handle_), gpu_info_,
                              gpu_feature_info_);
      DCHECK(!channel_handle_.is_valid());
    }
    return;
  }

  GpuHostImpl* gpu_host = delegate_->EnsureGpuHost();
  if (!gpu_host) {
    if (callback) {
      std::move(callback).Run(client_id_, mojo::ScopedMessagePipeHandle(),
                              gpu::GPUInfo(), gpu::GpuFeatureInfo());
    }
    return;
  }

  callback_ = std::move(callback);
  if (gpu_channel_requested_)
    return;
  gpu_channel_requested_ = true;
  const bool is_gpu_host = false;
  gpu_host->EstablishGpuChannel(
      client_id_, client_tracing_id_, is_gpu_host, false,
      base::BindOnce(&GpuClient::OnEstablishGpuChannel,
                     weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void GpuClient::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  if (auto* gpu_host = delegate_->EnsureGpuHost()) {
    gpu_host->gpu_service()->CreateJpegDecodeAccelerator(
        std::move(jda_receiver));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void GpuClient::CreateVideoEncodeAcceleratorProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_receiver) {
  if (auto* gpu_host = delegate_->EnsureGpuHost()) {
    gpu_host->gpu_service()->CreateVideoEncodeAcceleratorProvider(
        std::move(vea_provider_receiver));
  }
}

void GpuClient::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    mojom::GpuMemoryBufferFactory::CreateGpuMemoryBufferCallback callback) {
  auto* gpu_memory_buffer_manager = delegate_->GetGpuMemoryBufferManager();

  if (pending_create_callbacks_.find(id) != pending_create_callbacks_.end()) {
    gpu_memory_buffer_factory_receivers_.ReportBadMessage(
        "GpuMemoryBufferId already in use");
    return;
  }

  if (!IsSizeValid(size)) {
    gpu_memory_buffer_factory_receivers_.ReportBadMessage("Invalid GMB size");
    return;
  }

  if (!gpu_memory_buffer_manager) {
    std::move(callback).Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  pending_create_callbacks_[id] = std::move(callback);
  gpu_memory_buffer_manager->AllocateGpuMemoryBuffer(
      id, client_id_, size, format, usage, gpu::kNullSurfaceHandle,
      base::BindOnce(&GpuClient::OnCreateGpuMemoryBuffer,
                     weak_factory_.GetWeakPtr(), id));
}

void GpuClient::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id) {
  if (auto* gpu_memory_buffer_manager =
          delegate_->GetGpuMemoryBufferManager()) {
    gpu_memory_buffer_manager->DestroyGpuMemoryBuffer(id, client_id_);
  }
}

void GpuClient::CopyGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory,
    CopyGpuMemoryBufferCallback callback) {
  auto* gpu_memory_buffer_manager = delegate_->GetGpuMemoryBufferManager();

  if (!gpu_memory_buffer_manager) {
    std::move(callback).Run(false);
    return;
  }

  gpu_memory_buffer_manager->CopyGpuMemoryBufferAsync(
      std::move(buffer_handle), std::move(shared_memory), std::move(callback));
}

void GpuClient::CreateGpuMemoryBufferFactory(
    mojo::PendingReceiver<mojom::GpuMemoryBufferFactory> receiver) {
  gpu_memory_buffer_factory_receivers_.Add(this, std::move(receiver));
}

}  // namespace viz
