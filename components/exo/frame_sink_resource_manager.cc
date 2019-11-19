// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/frame_sink_resource_manager.h"

#include <utility>

#include "base/stl_util.h"

namespace exo {

FrameSinkResourceManager::FrameSinkResourceManager() {}
FrameSinkResourceManager::~FrameSinkResourceManager() {
  ClearAllCallbacks();
}

bool FrameSinkResourceManager::HasReleaseCallbackForResource(
    viz::ResourceId id) {
  return release_callbacks_.find(id) != release_callbacks_.end();
}
void FrameSinkResourceManager::SetResourceReleaseCallback(
    viz::ResourceId id,
    viz::ReleaseCallback callback) {
  DCHECK(!callback.is_null());
  release_callbacks_[id] = std::move(callback);
}
int FrameSinkResourceManager::AllocateResourceId() {
  return next_resource_id_++;
}

bool FrameSinkResourceManager::HasNoCallbacks() const {
  return release_callbacks_.empty();
}

void FrameSinkResourceManager::ReclaimResource(
    const viz::ReturnedResource& resource) {
  auto it = release_callbacks_.find(resource.id);
  if (it != release_callbacks_.end()) {
    std::move(it->second).Run(resource.sync_token, resource.lost);
    release_callbacks_.erase(it);
  }
}

void FrameSinkResourceManager::ClearAllCallbacks() {
  for (auto& callback : release_callbacks_)
    std::move(callback.second).Run(gpu::SyncToken(), true /* lost */);
  release_callbacks_.clear();
}

}  // namespace exo
