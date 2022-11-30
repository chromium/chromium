// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/returned_resource.h"
#include "ui/gfx/gpu_fence_handle.h"

namespace viz {

ReturnedResource::ReturnedResource(ResourceId id,
                                   gpu::SyncToken sync_token,
                                   gfx::GpuFenceHandle release_fence,
                                   int count,
                                   bool lost)
    : id(id),
      sync_token(sync_token),
      release_fence(std::move(release_fence)),
      count(count),
      lost(lost) {}

ReturnedResource::ReturnedResource() = default;

ReturnedResource::~ReturnedResource() = default;

ReturnedResource::ReturnedResource(ReturnedResource&& other) = default;

ReturnedResource& ReturnedResource::operator=(ReturnedResource&& other) =
    default;

}  // namespace viz
