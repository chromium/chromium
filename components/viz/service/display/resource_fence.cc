// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resource_fence.h"

#include <utility>

#include "components/viz/service/display/display_resource_provider.h"

namespace viz {

ResourceFence::ResourceFence(DisplayResourceProvider* resource_provider)
    : resource_provider_(resource_provider->GetWeakPtr()) {}

ResourceFence::~ResourceFence() = default;

void ResourceFence::TrackDeferredResource(ResourceId id) {
  deferred_resources_.emplace(id);
}

void ResourceFence::FencePassed() {
  if (auto* resource_provider = resource_provider_.get()) {
    // Disallow access to GPU thread for Android WebView since the fence will be
    // processed asynchronously after we exit the RenderThread runloop. This is
    // not needed for Chromium, but we can't distinguish between Chromium and
    // Android WebView here, so always disallow GPU thread access.
    DisplayResourceProvider::ScopedBatchReturnResources returner(
        resource_provider, /*allow_access_to_gpu_thread=*/false);
    resource_provider->OnResourceFencePassed(this,
                                             std::move(deferred_resources_));
  }
}

}  // namespace viz
