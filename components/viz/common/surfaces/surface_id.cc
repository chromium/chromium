// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"

#include "base/strings/stringprintf.h"

namespace viz {

std::string SurfaceId::ToString() const {
  return base::StringPrintf("SurfaceId(%s, %s)",
                            frame_sink_id_.ToString().c_str(),
                            local_surface_id_.ToString().c_str());
}

SurfaceId SurfaceId::ToSmallestId() const {
  return SurfaceId(frame_sink_id_, local_surface_id_.ToSmallestId());
}

std::ostream& operator<<(std::ostream& out, const SurfaceId& surface_id) {
  return out << surface_id.ToString();
}

bool SurfaceId::IsNewerThan(const SurfaceId& other) const {
  if (frame_sink_id_ != other.frame_sink_id())
    return false;
  return local_surface_id_.IsNewerThan(other.local_surface_id());
}

bool SurfaceId::IsSameOrNewerThan(const SurfaceId& other) const {
  return (*this == other) || IsNewerThan(other);
}

}  // namespace viz
