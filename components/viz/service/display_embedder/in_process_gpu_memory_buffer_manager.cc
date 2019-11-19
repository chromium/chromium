// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/in_process_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

namespace viz {
namespace {

void DestroyOnThread(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     gpu::GpuMemoryBufferImpl::DestructionCallback callback,
                     const gpu::SyncToken& sync_token) {
  if (task_runner->BelongsToCurrentThread()) {
    std::move(callback).Run(sync_token);
  } else {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), sync_token));
  }
}

}  // namespace

InProcessGpuMemoryBufferManager::InProcessGpuMemoryBufferManager(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    gpu::SyncPointManager* sync_point_manager)
    : client_id_(gpu::kInProcessCommandBufferClientId),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      sync_point_manager_(sync_point_manager),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "InProcessGpuMemoryBufferManager", task_runner_);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

InProcessGpuMemoryBufferManager::~InProcessGpuMemoryBufferManager() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
InProcessGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  gfx::GpuMemoryBufferId id(next_gpu_memory_id_++);
  gfx::GpuMemoryBufferHandle buffer_handle =
      gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
          id, size, format, usage, client_id_, surface_handle);

  AllocatedBufferInfo buffer_info(buffer_handle, size, format);

  auto callback = base::BindOnce(
      &InProcessGpuMemoryBufferManager::ShouldDestroyGpuMemoryBuffer, weak_ptr_,
      id);
  auto gmb = gpu_memory_buffer_support_.CreateGpuMemoryBufferImplFromHandle(
      std::move(buffer_handle), size, format, usage,
      base::BindOnce(&DestroyOnThread, task_runner_, std::move(callback)));

  if (gmb)
    allocated_buffers_.insert(std::make_pair(id, buffer_info));

  return gmb;
}

void InProcessGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  static_cast<gpu::GpuMemoryBufferImpl*>(buffer)->set_destruction_sync_token(
      sync_token);
}

bool InProcessGpuMemoryBufferManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  uint64_t client_tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();

  for (auto& buffer_pair : allocated_buffers_) {
    auto& buffer_info = buffer_pair.second;
    if (!buffer_info.OnMemoryDump(pmd, client_id_, client_tracing_process_id))
      return false;
  }

  return true;
}

void InProcessGpuMemoryBufferManager::ShouldDestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gpu::SyncToken& sync_token) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto callback = base::BindOnce(
      &InProcessGpuMemoryBufferManager::DestroyGpuMemoryBuffer, weak_ptr_, id);
  // This is equivalent to calling SyncPointerManager::WaitOutOfOrder() except
  // |callback| will be run on this thread instead of the thread where the sync
  // point is released.
  bool will_run = sync_point_manager_->WaitNonThreadSafe(
      sync_token, gpu::SequenceId(), UINT32_MAX, task_runner_,
      std::move(callback));

  // No sync token or invalid sync token, destroy immediately.
  if (!will_run)
    DestroyGpuMemoryBuffer(id);
}

void InProcessGpuMemoryBufferManager::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id) {
  allocated_buffers_.erase(id);
  gpu_memory_buffer_factory_->DestroyGpuMemoryBuffer(id, client_id_);
}

}  // namespace viz
