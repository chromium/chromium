// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/surface_range.h"

#include <ostream>
#include <string>

#include "base/strings/stringprintf.h"

namespace viz {

SurfaceRange::SurfaceRange() = default;

SurfaceRange::SurfaceRange(const std::optional<SurfaceId>& start,
                           const SurfaceId& end)
    : start_(start), end_(end) {}

SurfaceRange::SurfaceRange(const SurfaceId& surface_id)
    : start_(surface_id), end_(surface_id) {}

SurfaceRange::SurfaceRange(const SurfaceRange& other) = default;

SurfaceRange& SurfaceRange::operator=(const SurfaceRange& other) = default;

bool SurfaceRange::IsInRangeExclusive(const SurfaceId& surface_id) const {
  if (!start_)
    return end_.IsNewerThan(surface_id);

  if (HasDifferentFrameSinkIds() ||
      end_.local_surface_id().embed_token() !=
          start_->local_surface_id().embed_token()) {
    return surface_id.IsNewerThan(*start_) || end_.IsNewerThan(surface_id);
  }

  return surface_id.IsNewerThan(*start_) && end_.IsNewerThan(surface_id);
}

bool SurfaceRange::IsInRangeInclusive(const SurfaceId& surface_id) const {
  return IsInRangeExclusive(surface_id) || end_ == surface_id ||
         start_ == surface_id;
}

bool SurfaceRange::HasDifferentFrameSinkIds() const {
  return start_ && start_->frame_sink_id() != end_.frame_sink_id();
}

bool SurfaceRange::HasDifferentEmbedTokens() const {
  return start_ && start_->local_surface_id().embed_token() !=
                       end_.local_surface_id().embed_token();
}

bool SurfaceRange::IsValid() const {
  if (!end_.is_valid())
    return false;

  if (!start_)
    return true;

  if (!start_->is_valid())
    return false;

  // The start/end SurfaceIds can have a different FrameSinkId or embed token
  // for cross SiteInstanceGroup navigations.
  if (end_.frame_sink_id() != start_->frame_sink_id())
    return true;

  if (end_.local_surface_id().embed_token() !=
      start_->local_surface_id().embed_token()) {
    return true;
  }

  return end_.local_surface_id().IsSameOrNewerThan(start_->local_surface_id());
}

std::string SurfaceRange::ToString() const {
  return base::StringPrintf("SurfaceRange(start: %s, end: %s)",
                            start_ ? start_->ToString().c_str() : "none",
                            end_.ToString().c_str());
}

std::ostream& operator<<(std::ostream& out, const SurfaceRange& surface_range) {
  return out << surface_range.ToString();
}

}  // namespace viz
