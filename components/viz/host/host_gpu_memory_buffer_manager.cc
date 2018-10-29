// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"
#include "ui/gfx/buffer_format_util.h"

namespace viz {

namespace {

void OnGpuMemoryBufferDestroyed(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const gpu::GpuMemoryBufferImpl::DestructionCallback& callback,
    const gpu::SyncToken& sync_token) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(callback, sync_token));
}

}  // namespace

HostGpuMemoryBufferManager::PendingBufferInfo::PendingBufferInfo() = default;
HostGpuMemoryBufferManager::PendingBufferInfo::PendingBufferInfo(
    PendingBufferInfo&&) = default;
HostGpuMemoryBufferManager::PendingBufferInfo::~PendingBufferInfo() = default;

HostGpuMemoryBufferManager::AllocatedBufferInfo::AllocatedBufferInfo() =
    default;
HostGpuMemoryBufferManager::AllocatedBufferInfo::~AllocatedBufferInfo() =
    default;

HostGpuMemoryBufferManager::HostGpuMemoryBufferManager(
    GpuServiceProvider gpu_service_provider,
    int client_id,
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : gpu_service_provider_(gpu_service_provider),
      client_id_(client_id),
      gpu_memory_buffer_support_(std::move(gpu_memory_buffer_support)),
      native_configurations_(gpu::GetNativeGpuMemoryBufferConfigurations(
          gpu_memory_buffer_support_.get())),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
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
  DCHECK_NE(gfx::EMPTY_BUFFER, buffer_iter->second.type);
  if (buffer_iter->second.type != gfx::SHARED_MEMORY_BUFFER) {
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
      DCHECK_NE(gfx::EMPTY_BUFFER, pair.second.type);
      if (pair.second.type != gfx::SHARED_MEMORY_BUFFER) {
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
    AllocatedBufferInfo buffer_info;
    DCHECK_EQ(gfx::SHARED_MEMORY_BUFFER, buffer_handle.type);
    buffer_info.type = gfx::SHARED_MEMORY_BUFFER;
    buffer_info.buffer_size_in_bytes =
        gfx::BufferSizeForBufferFormat(size, format);
    buffer_info.shared_memory_guid = buffer_handle.region.GetGUID();
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
  return native_configurations_.find(std::make_pair(format, usage)) !=
         native_configurations_.end();
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
      base::BindRepeating(
          &OnGpuMemoryBufferDestroyed, task_runner_,
          base::BindRepeating(
              &HostGpuMemoryBufferManager::DestroyGpuMemoryBuffer, weak_ptr_,
              id, client_id_)));
}

void HostGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

bool HostGpuMemoryBufferManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  for (const auto& pair : allocated_buffers_) {
    int client_id = pair.first;
    for (const auto& buffer_pair : pair.second) {
      gfx::GpuMemoryBufferId buffer_id = buffer_pair.first;
      const AllocatedBufferInfo& buffer_info = buffer_pair.second;
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(base::StringPrintf(
              "gpumemorybuffer/client_%d/buffer_%d", client_id, buffer_id.id));
      if (!dump)
        return false;
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      buffer_info.buffer_size_in_bytes);

      // Create the cross-process ownership edge. If the client creates a
      // corresponding dump for the same buffer, this will avoid to
      // double-count them in tracing. If, instead, no other process will emit a
      // dump with the same guid, the segment will be accounted to the browser.
      uint64_t client_tracing_process_id = ClientIdToTracingId(client_id);

      if (buffer_info.type == gfx::SHARED_MEMORY_BUFFER) {
        pmd->CreateSharedMemoryOwnershipEdge(
            dump->guid(), buffer_info.shared_memory_guid, 0 /* importance */);
      } else {
        auto shared_buffer_guid = gfx::GetGenericSharedGpuMemoryGUIDForTracing(
            client_tracing_process_id, buffer_id);
        pmd->CreateSharedGlobalAllocatorDump(shared_buffer_guid);
        pmd->AddOwnershipEdge(dump->guid(), shared_buffer_guid);
      }
    }
  }
  return true;
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
  return static_cast<uint64_t>(base::Hash(&client_id, sizeof(client_id))) + 1;
}

void HostGpuMemoryBufferManager::OnGpuMemoryBufferAllocated(
    int gpu_service_version,
    int client_id,
    gfx::GpuMemoryBufferId id,
    gfx::GpuMemoryBufferHandle handle) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If the current GPU service is different from the one that alloacted the
  // buffer, the buffer should be considered as stale.
  bool stale = gpu_service_version_ != gpu_service_version;

  auto client_iter = pending_buffers_.find(client_id);
  if (client_iter == pending_buffers_.end()) {
    // The client has been destroyed since the allocation request was made. The
    // callback is already called with null handle.
    if (!handle.is_null() && !stale) {
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

  if (stale) {
    // Try re-allocating buffer on the new GPU service.
    AllocateGpuMemoryBuffer(id, client_id, pending_buffer.size,
                            pending_buffer.format, pending_buffer.usage,
                            pending_buffer.surface_handle,
                            std::move(pending_buffer.callback));
    return;
  }

  if (!handle.is_null()) {
    DCHECK(handle.id == id);

    AllocatedBufferInfo buffer_info;
    buffer_info.type = handle.type;
    buffer_info.buffer_size_in_bytes = gfx::BufferSizeForBufferFormat(
        pending_buffer.size, pending_buffer.format);
    allocated_buffers_[client_id].insert(std::make_pair(id, buffer_info));
  }
  std::move(pending_buffer.callback).Run(std::move(handle));
}

}  // namespace viz
