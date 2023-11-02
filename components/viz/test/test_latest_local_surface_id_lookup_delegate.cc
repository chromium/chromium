// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_latest_local_surface_id_lookup_delegate.h"

namespace viz {

TestLatestLocalSurfaceIdLookupDelegate::
    TestLatestLocalSurfaceIdLookupDelegate() = default;
TestLatestLocalSurfaceIdLookupDelegate::
    ~TestLatestLocalSurfaceIdLookupDelegate() = default;

LocalSurfaceId TestLatestLocalSurfaceIdLookupDelegate::GetSurfaceAtAggregation(
    const FrameSinkId& frame_sink_id) const {
  auto it = surface_id_map_.find(frame_sink_id);
  if (it == surface_id_map_.end())
    return LocalSurfaceId();
  return it->second;
}

void TestLatestLocalSurfaceIdLookupDelegate::SetSurfaceIdMap(
    const SurfaceId& surface_id) {
  surface_id_map_[surface_id.frame_sink_id()] = surface_id.local_surface_id();
}

}  // namespace viz
