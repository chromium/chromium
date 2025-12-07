// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/frame_sink_resource_manager.h"

#include <utility>


namespace exo {

FrameSinkResourceManager::FrameSinkResourceManager() = default;
FrameSinkResourceManager::~FrameSinkResourceManager() {
  ClearAllCallbacks();
}

bool FrameSinkResourceManager::HasReleaseCallbackForResource(
    viz::ResourceId id) {
  return release_callbacks_.find(id) != release_callbacks_.end();
}
void FrameSinkResourceManager::SetResourceReleaseCallback(
    viz::ResourceId id,
    ReleaseCallback callback) {
  DCHECK(!callback.is_null());
  release_callbacks_[id] = std::move(callback);
}
viz::ResourceId FrameSinkResourceManager::AllocateResourceId() {
  return id_generator_.GenerateNextId();
}

bool FrameSinkResourceManager::HasNoCallbacks() const {
  return release_callbacks_.empty();
}

void FrameSinkResourceManager::ReclaimResource(viz::ReturnedResource resource) {
  auto it = release_callbacks_.find(resource.id);
  if (it != release_callbacks_.end()) {
    std::move(it->second).Run(std::move(resource));
    release_callbacks_.erase(it);
  }
}

void FrameSinkResourceManager::ClearAllCallbacks() {
  for (auto& callback : release_callbacks_)
    std::move(callback.second)
        .Run(viz::ReturnedResource(viz::kInvalidResourceId, gpu::SyncToken(),
                                   /*release_fence=*/gfx::GpuFenceHandle(),
                                   /*count=*/0, /*lost=*/true));
  release_callbacks_.clear();
}

}  // namespace exo
