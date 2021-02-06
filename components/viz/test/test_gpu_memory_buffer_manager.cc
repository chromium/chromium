// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gpu_memory_buffer_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {
namespace {

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(TestGpuMemoryBufferManager* manager,
                      int id,
                      const gfx::Size& size,
                      gfx::BufferFormat format,
                      base::UnsafeSharedMemoryRegion shared_memory_region,
                      size_t offset,
                      size_t stride)
      : manager_(manager),
        id_(id),
        size_(size),
        format_(format),
        region_(std::move(shared_memory_region)),
        offset_(offset),
        stride_(stride),
        mapped_(false) {}

  ~GpuMemoryBufferImpl() override { manager_->OnGpuMemoryBufferDestroyed(id_); }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    DCHECK_EQ(stride_, gfx::RowSizeForBufferFormat(size_.width(), format_, 0));
    mapping_ = region_.MapAt(
        0, offset_ + gfx::BufferSizeForBufferFormat(size_, format_));
    if (!mapping_.IsValid())
      return false;
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return reinterpret_cast<uint8_t*>(mapping_.memory()) + offset_ +
           gfx::BufferOffsetForBufferFormat(size_, format_, plane);
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapping_ = base::WritableSharedMemoryMapping();
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format_));
    return base::checked_cast<int>(gfx::RowSizeForBufferFormat(
        size_.width(), format_, static_cast<int>(plane)));
  }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::SHARED_MEMORY_BUFFER;
  }
  gfx::GpuMemoryBufferId GetId() const override { return id_; }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    gfx::GpuMemoryBufferHandle handle;
    handle.type = gfx::SHARED_MEMORY_BUFFER;
    handle.region = region_.Duplicate();
    handle.offset = base::checked_cast<uint32_t>(offset_);
    handle.stride = base::checked_cast<int32_t>(stride_);
    return handle;
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

 private:
  TestGpuMemoryBufferManager* manager_;
  gfx::GpuMemoryBufferId id_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
  base::UnsafeSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;
  size_t offset_;
  size_t stride_;
  bool mapped_;
};

class GpuMemoryBufferFromClient : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferFromClient(TestGpuMemoryBufferManager* manager,
                            int id,
                            gfx::GpuMemoryBuffer* client_buffer)
      : manager_(manager), id_(id), client_buffer_(client_buffer) {}

  ~GpuMemoryBufferFromClient() override {
    manager_->OnGpuMemoryBufferDestroyed(id_);
  }

  bool Map() override { return client_buffer_->Map(); }
  void* memory(size_t plane) override { return client_buffer_->memory(plane); }
  void Unmap() override { client_buffer_->Unmap(); }
  gfx::Size GetSize() const override { return client_buffer_->GetSize(); }
  gfx::BufferFormat GetFormat() const override {
    return client_buffer_->GetFormat();
  }
  int stride(size_t plane) const override {
    return client_buffer_->stride(plane);
  }
  gfx::GpuMemoryBufferId GetId() const override { return id_; }
  gfx::GpuMemoryBufferType GetType() const override {
    return client_buffer_->GetType();
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    return client_buffer_->CloneHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return client_buffer_->AsClientBuffer();
  }
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

 private:
  TestGpuMemoryBufferManager* manager_;
  gfx::GpuMemoryBufferId id_;
  gfx::GpuMemoryBuffer* client_buffer_;
};

}  // namespace

TestGpuMemoryBufferManager::TestGpuMemoryBufferManager() {}

TestGpuMemoryBufferManager::~TestGpuMemoryBufferManager() {
  base::AutoLock hold(lock_);
  DCHECK(buffers_.empty());
  DCHECK(clients_.empty());
  if (parent_gpu_memory_buffer_manager_)
    parent_gpu_memory_buffer_manager_->clients_.erase(client_id_);
}

std::unique_ptr<TestGpuMemoryBufferManager>
TestGpuMemoryBufferManager::CreateClientGpuMemoryBufferManager() {
  base::AutoLock hold(lock_);
  std::unique_ptr<TestGpuMemoryBufferManager> client(
      new TestGpuMemoryBufferManager);
  client->client_id_ = ++last_client_id_;
  client->parent_gpu_memory_buffer_manager_ = this;

  clients_[client->client_id_] = client.get();
  return client;
}

void TestGpuMemoryBufferManager::OnGpuMemoryBufferDestroyed(
    gfx::GpuMemoryBufferId gpu_memory_buffer_id) {
  base::AutoLock hold(lock_);
  DCHECK(buffers_.find(gpu_memory_buffer_id.id) != buffers_.end());
  buffers_.erase(gpu_memory_buffer_id.id);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
TestGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  base::AutoLock hold(lock_);

  if (fail_on_create_)
    return nullptr;
  const size_t buffer_size = gfx::BufferSizeForBufferFormat(size, format);
  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  if (!shared_memory_region.IsValid())
    return nullptr;

  last_gpu_memory_buffer_id_ += 1;
  std::unique_ptr<gfx::GpuMemoryBuffer> result(new GpuMemoryBufferImpl(
      this, last_gpu_memory_buffer_id_, size, format,
      std::move(shared_memory_region), 0,
      base::checked_cast<int>(
          gfx::RowSizeForBufferFormat(size.width(), format, 0))));
  buffers_[last_gpu_memory_buffer_id_] = result.get();
  return result;
}

void TestGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {}

}  // namespace viz
