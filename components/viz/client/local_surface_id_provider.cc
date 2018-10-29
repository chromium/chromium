// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/local_surface_id_provider.h"

#include "components/viz/common/quads/compositor_frame.h"

namespace viz {

LocalSurfaceIdProvider::LocalSurfaceIdProvider() = default;

LocalSurfaceIdProvider::~LocalSurfaceIdProvider() = default;

void LocalSurfaceIdProvider::ForceAllocateNewId() {
  parent_local_surface_id_allocator_.GenerateId();
}

DefaultLocalSurfaceIdProvider::DefaultLocalSurfaceIdProvider() = default;

const LocalSurfaceId& DefaultLocalSurfaceIdProvider::GetLocalSurfaceIdForFrame(
    const CompositorFrame& frame) {
  if (frame.size_in_pixels() != surface_size_ ||
      frame.device_scale_factor() != device_scale_factor_) {
    parent_local_surface_id_allocator_.GenerateId();
  }
  surface_size_ = frame.size_in_pixels();
  device_scale_factor_ = frame.device_scale_factor();
  return parent_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

}  // namespace viz
