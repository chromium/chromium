// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_context_support.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"

namespace viz {

TestContextSupport::TestContextSupport() : out_of_order_callbacks_(false) {}

TestContextSupport::~TestContextSupport() = default;

void TestContextSupport::FlushPendingWork() {}

void TestContextSupport::SignalSyncToken(const gpu::SyncToken& sync_token,
                                         base::OnceClosure callback) {
  sync_point_callbacks_.push_back(std::move(callback));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestContextSupport::CallAllSyncPointCallbacks,
                                weak_ptr_factory_.GetWeakPtr()));
}

bool TestContextSupport::IsSyncTokenSignaled(const gpu::SyncToken& sync_token) {
  return true;
}

void TestContextSupport::SignalQuery(uint32_t query,
                                     base::OnceClosure callback) {
  sync_point_callbacks_.push_back(std::move(callback));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TestContextSupport::CallAllSyncPointCallbacks,
                                weak_ptr_factory_.GetWeakPtr()));
}

void TestContextSupport::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {}

void TestContextSupport::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  aggressively_free_resources_ = aggressively_free_resources;
}

bool TestContextSupport::GetAggressivelyFreeResources() const {
  return aggressively_free_resources_;
}

void TestContextSupport::CallAllSyncPointCallbacks() {
  size_t size = sync_point_callbacks_.size();
  if (out_of_order_callbacks_) {
    for (size_t i = size; i > 0; --i) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(sync_point_callbacks_[i - 1]));
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(sync_point_callbacks_[i]));
    }
  }
  sync_point_callbacks_.clear();
}

void TestContextSupport::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> callback) {}

base::span<uint8_t> TestContextSupport::MapTransferCacheEntry(
    uint32_t serialized_size) {
  NOTIMPLEMENTED();
  return {};
}

void TestContextSupport::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                          uint32_t id) {
  NOTIMPLEMENTED();
}

bool TestContextSupport::ThreadsafeLockTransferCacheEntry(uint32_t entry_type,
                                                          uint32_t entry_id) {
  NOTIMPLEMENTED();
  return false;
}
void TestContextSupport::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  NOTIMPLEMENTED();
}
void TestContextSupport::DeleteTransferCacheEntry(uint32_t entry_type,
                                                  uint32_t entry_id) {
  NOTIMPLEMENTED();
}
unsigned int TestContextSupport::GetTransferBufferFreeSize() const {
  NOTIMPLEMENTED();
  return 0;
}
}  // namespace viz
