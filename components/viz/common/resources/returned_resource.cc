// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/returned_resource.h"

#include <utility>

#include "ui/gfx/gpu_fence_handle.h"

namespace viz {

// TODO(crbug.com/40286368): Store the SharedImageExportResult directly in
// ReturnedResource.
ReturnedResource::ReturnedResource(
    ResourceId id,
    gpu::SharedImageExportResult shared_image_export_result,
    gfx::GpuFenceHandle release_fence,
    int count,
    bool lost)
    : id(id),
      sync_token(shared_image_export_result.sync_token_),
      release_fence(std::move(release_fence)),
      count(count),
      lost(lost) {}

ReturnedResource::ReturnedResource() = default;

ReturnedResource::~ReturnedResource() = default;

ReturnedResource::ReturnedResource(ReturnedResource&& other) = default;

ReturnedResource& ReturnedResource::operator=(ReturnedResource&& other) =
    default;

ReturnedResourceViz::ReturnedResourceViz(ResourceId id,
                                         gpu::SyncToken sync_token,
                                         gfx::GpuFenceHandle release_fence,
                                         int count,
                                         bool lost)
    : id(id),
      sync_token(sync_token),
      release_fence(std::move(release_fence)),
      count(count),
      lost(lost) {}

ReturnedResourceViz::ReturnedResourceViz() = default;

ReturnedResourceViz::~ReturnedResourceViz() = default;

ReturnedResourceViz::ReturnedResourceViz(ReturnedResourceViz&& other) = default;

ReturnedResourceViz& ReturnedResourceViz::operator=(
    ReturnedResourceViz&& other) = default;

}  // namespace viz
