// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {

namespace {

void OnGpuMemoryBufferDestroyed(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gpu::GpuMemoryBufferImpl::DestructionCallback callback,
    const gpu::SyncToken& sync_token) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), sync_token));
}

bool WillGetGmbConfigFromGpu() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    // Ozone/X11 (same as non-Ozone/X11) cannot get buffer formats in the
    // browser process and requires gpu initialization to be done before it can
    // determine what formats gmb can use. This limitation comes from the
    // requirement to have GLX bindings initialized. The buffer formats will be
    // passed through gpu extra info.
    return ui::OzonePlatform::GetInstance()
        ->GetPlatformProperties()
        .fetch_buffer_formats_for_gmb_on_gpu;
  }
#endif
#if defined(USE_X11)
  // non-Ozone/X11 must always get native configs on gpu.
  DCHECK(!features::IsUsingOzonePlatform());
  return true;
#endif
  return false;
}

}  // namespace

HostGpuMemoryBufferManager::PendingBufferInfo::PendingBufferInfo() = default;
HostGpuMemoryBufferManager::PendingBufferInfo::PendingBufferInfo(
    PendingBufferInfo&&) = default;
HostGpuMemoryBufferManager::PendingBufferInfo::~PendingBufferInfo() = default;

HostGpuMemoryBufferManager::HostGpuMemoryBufferManager(
    GpuServiceProvider gpu_service_provider,
    int client_id,
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : gpu_service_provider_(gpu_service_provider),
      client_id_(client_id),
      gpu_memory_buffer_support_(std::move(gpu_memory_buffer_support)),
      task_runner_(std::move(task_runner)) {
  if (!WillGetGmbConfigFromGpu()) {
    native_configurations_ = gpu::GetNativeGpuMemoryBufferConfigurations(
        gpu_memory_buffer_support_.get());
    native_configurations_initialized_.Signal();
  }
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "HostGpuMemoryBufferManager", task_runner_);
}

HostGpuMemoryBufferManager::~HostGpuMemoryBufferManager() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void HostGpuMemoryBufferManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto client_iter = allocated_buffers_.find(client_id);
  if (client_iter == allocated_buffers_.end())
    return;
  auto& buffers = client_iter->second;
  auto buffer_iter = buffers.find(id);
  if (buffer_iter == buffers.end())
    return;
  DCHECK_NE(gfx::EMPTY_BUFFER, buffer_iter->second.type());
  if (buffer_iter->second.type() != gfx::SHARED_MEMORY_BUFFER) {
    auto* gpu_service = GetGpuService();
    DCHECK(gpu_service);
    gpu_service->DestroyGpuMemoryBuffer(id, client_id, sync_token);
  }
  buffers.erase(buffer_iter);
}

void HostGpuMemoryBufferManager::DestroyAllGpuMemoryBufferForClient(
    int client_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto client_iter = allocated_buffers_.find(client_id);
  if (client_iter != allocated_buffers_.end()) {
    auto& buffers = client_iter->second;
    for (const auto& pair : buffers) {
      DCHECK_NE(gfx::EMPTY_BUFFER, pair.second.type());
      if (pair.second.type() != gfx::SHARED_MEMORY_BUFFER) {
        auto* gpu_service = GetGpuService();
        DCHECK(gpu_service);
        gpu_service->DestroyGpuMemoryBuffer(pair.first, client_id,
                                            gpu::SyncToken());
      }
    }
    allocated_buffers_.erase(client_iter);
  }
  auto pending_client_iter = pending_buffers_.find(client_id);
  if (pending_client_iter != pending_buffers_.end()) {
    auto& buffers = pending_client_iter->second;
    for (auto& pair : buffers)
      std::move(pair.second.callback).Run(gfx::GpuMemoryBufferHandle());
    pending_buffers_.erase(pending_client_iter);
  }
}

void HostGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    base::OnceCallback<void(gfx::GpuMemoryBufferHandle)> callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!weak_ptr_)
    weak_ptr_ = weak_factory_.GetWeakPtr();
  if (gpu_memory_buffer_support_->GetNativeGpuMemoryBufferType() !=
          gfx::EMPTY_BUFFER &&
      IsNativeGpuMemoryBufferConfiguration(format, usage)) {
    if (auto* gpu_service = GetGpuService()) {
      PendingBufferInfo buffer_info;
      buffer_info.size = size;
      buffer_info.format = format;
      buffer_info.usage = usage;
      buffer_info.surface_handle = surface_handle;
      buffer_info.callback = std::move(callback);
      pending_buffers_[client_id].insert(
          std::make_pair(id, std::move(buffer_info)));
      gpu_service->CreateGpuMemoryBuffer(
          id, size, format, usage, client_id, surface_handle,
          base::BindOnce(
              &HostGpuMemoryBufferManager::OnGpuMemoryBufferAllocated,
              weak_ptr_, gpu_service_version_, client_id, id));
    } else {
      // GPU service failed to start. Run the callback with null handle.
      std::move(callback).Run(gfx::GpuMemoryBufferHandle());
    }
    return;
  }

  gfx::GpuMemoryBufferHandle buffer_handle;
  // The requests are coming in from untrusted clients. So verify that it is
  // possible to allocate shared memory buffer first.
  if (gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage) &&
      gpu::GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(size,
                                                                 format)) {
    buffer_handle = gpu::GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
        id, size, format, usage);
    DCHECK_EQ(gfx::SHARED_MEMORY_BUFFER, buffer_handle.type);
    AllocatedBufferInfo buffer_info(buffer_handle, size, format);
    allocated_buffers_[client_id].insert(
        std::make_pair(buffer_handle.id, buffer_info));
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(buffer_handle)));
}

bool HostGpuMemoryBufferManager::IsNativeGpuMemoryBufferConfiguration(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    native_configurations_initialized_.Wait();
  }
  return native_configurations_.find(gfx::BufferUsageAndFormat(
             usage, format)) != native_configurations_.end();
}

std::unique_ptr<gfx::GpuMemoryBuffer>
HostGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  gfx::GpuMemoryBufferId id(next_gpu_memory_id_++);
  gfx::GpuMemoryBufferHandle handle;
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  DCHECK(!task_runner_->BelongsToCurrentThread());
  auto reply_callback = base::BindOnce(
      [](gfx::GpuMemoryBufferHandle* handle, base::WaitableEvent* wait_event,
         gfx::GpuMemoryBufferHandle allocated_buffer_handle) {
        *handle = std::move(allocated_buffer_handle);
        wait_event->Signal();
      },
      &handle, &wait_event);
  // We block with a WaitableEvent until the callback is run. So using
  // base::Unretained() is safe here.
  auto allocate_callback =
      base::BindOnce(&HostGpuMemoryBufferManager::AllocateGpuMemoryBuffer,
                     base::Unretained(this), id, client_id_, size, format,
                     usage, surface_handle, std::move(reply_callback));
  task_runner_->PostTask(FROM_HERE, std::move(allocate_callback));
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope
      allow_base_sync_primitives;
  wait_event.Wait();
  if (handle.is_null())
    return nullptr;
  // The destruction callback can be called on any thread. So use an
  // intermediate callback here as the destruction callback, which bounces off
  // onto the |task_runner_| thread to do the real work.
  return gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
      std::move(handle), size, format, usage,
      base::BindOnce(
          &OnGpuMemoryBufferDestroyed, task_runner_,
          base::BindOnce(&HostGpuMemoryBufferManager::DestroyGpuMemoryBuffer,
                         weak_ptr_, id, client_id_)));
}

void HostGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

void HostGpuMemoryBufferManager::CopyGpuMemoryBufferAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  if (auto* gpu_service = GetGpuService()) {
    gpu_service->CopyGpuMemoryBuffer(std::move(buffer_handle),
                                     std::move(memory_region),
                                     std::move(callback));
  } else {
    // GPU service failed to start. Run the callback with a null handle.
    std::move(callback).Run(false);
  }
}

bool HostGpuMemoryBufferManager::CopyGpuMemoryBufferSync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region) {
  NOTIMPLEMENTED();
  return false;
}

bool HostGpuMemoryBufferManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  for (const auto& pair : allocated_buffers_) {
    int client_id = pair.first;
    uint64_t client_tracing_process_id = ClientIdToTracingId(client_id);
    for (const auto& buffer_pair : pair.second) {
      auto& buffer_info = buffer_pair.second;
      if (!buffer_info.OnMemoryDump(pmd, client_id, client_tracing_process_id))
        return false;
    }
  }
  return true;
}

void HostGpuMemoryBufferManager::SetNativeConfigurations(
    gpu::GpuMemoryBufferConfigurationSet native_configurations) {
  // Must not be done on the task runner thread to avoid deadlock.
  DCHECK(!task_runner_->BelongsToCurrentThread());
  if (native_configurations_initialized_.IsSignaled()) {
    // The configurations are set on GPU initialization and should not change.
    DCHECK(native_configurations_ == native_configurations);
  } else {
    native_configurations_ = native_configurations;
    native_configurations_initialized_.Signal();
  }
}

mojom::GpuService* HostGpuMemoryBufferManager::GetGpuService() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (gpu_service_)
    return gpu_service_;

  gpu_service_ = gpu_service_provider_.Run(base::BindOnce(
      &HostGpuMemoryBufferManager::OnConnectionError, weak_ptr_));
  return gpu_service_;
}

void HostGpuMemoryBufferManager::OnConnectionError() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  gpu_service_ = nullptr;
  gpu_service_version_++;

  // Drop allocated buffers.
  allocated_buffers_.clear();

  // Retry requesting pending buffer allocations.
  auto pending_buffers = std::move(pending_buffers_);
  pending_buffers_.clear();
  for (auto& client_pair : pending_buffers) {
    for (auto& buffer_pair : client_pair.second) {
      auto& buffer = buffer_pair.second;
      LOG(WARNING) << "Retrying allocation of GpuMemoryBuffer with id = "
                   << buffer_pair.first.id
                   << ", client_id = " << client_pair.first
                   << ", size = " << buffer.size.ToString()
                   << ", format = " << gfx::BufferFormatToString(buffer.format)
                   << ", usage = " << gfx::BufferUsageToString(buffer.usage)
                   << ", surface_handle = "
                   << buffer.surface_handle
                   << " due to connection error";
      AllocateGpuMemoryBuffer(
          buffer_pair.first, client_pair.first, buffer.size, buffer.format,
          buffer.usage, buffer.surface_handle, std::move(buffer.callback));
    }
  }
}

uint64_t HostGpuMemoryBufferManager::ClientIdToTracingId(int client_id) const {
  if (client_id == client_id_) {
    return base::trace_event::MemoryDumpManager::GetInstance()
        ->GetTracingProcessId();
  }
  // TODO(sad|ssid): Find a better way once https://crbug.com/661257 is
  // resolved.  The hash value is incremented so that the tracing id is never
  // equal to MemoryDumpManager::kInvalidTracingProcessId.
  return static_cast<uint64_t>(base::PersistentHash(
             base::as_bytes(base::make_span(&client_id, 1)))) +
         1;
}

void HostGpuMemoryBufferManager::OnGpuMemoryBufferAllocated(
    int gpu_service_version,
    int client_id,
    gfx::GpuMemoryBufferId id,
    gfx::GpuMemoryBufferHandle handle) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If the buffer is allocated by an old gpu service, we can safely ignore it
  // as we have already requested a new one on the new gpu service in
  // OnConnectionError().
  if (gpu_service_version_ != gpu_service_version)
    return;

  auto client_iter = pending_buffers_.find(client_id);
  if (client_iter == pending_buffers_.end()) {
    // The client has been destroyed since the allocation request was made. The
    // callback is already called with null handle.
    if (!handle.is_null()) {
      auto* gpu_service = GetGpuService();
      DCHECK(gpu_service);
      gpu_service->DestroyGpuMemoryBuffer(handle.id, client_id,
                                          gpu::SyncToken());
    }
    return;
  }

  auto buffer_iter = client_iter->second.find(id);
  DCHECK(buffer_iter != client_iter->second.end());
  PendingBufferInfo pending_buffer = std::move(buffer_iter->second);
  client_iter->second.erase(buffer_iter);

  if (!handle.is_null()) {
    DCHECK(handle.id == id);

    AllocatedBufferInfo buffer_info(handle, pending_buffer.size,
                                    pending_buffer.format);
    allocated_buffers_[client_id].insert(std::make_pair(id, buffer_info));
  }
  std::move(pending_buffer.callback).Run(std::move(handle));
}

}  // namespace viz
