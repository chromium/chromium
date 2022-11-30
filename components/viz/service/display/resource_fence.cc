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
  if (resource_provider_) {
    resource_provider_->OnResourceFencePassed(this,
                                              std::move(deferred_resources_));
  }
}

}  // namespace viz
