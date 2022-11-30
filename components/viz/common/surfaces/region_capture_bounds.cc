// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/region_capture_bounds.h"

#include <sstream>
#include <utility>

#include "base/no_destructor.h"

namespace viz {

RegionCaptureBounds::RegionCaptureBounds() = default;
RegionCaptureBounds::RegionCaptureBounds(
    base::flat_map<base::Token, gfx::Rect> bounds)
    : bounds_(std::move(bounds)) {}
RegionCaptureBounds::RegionCaptureBounds(RegionCaptureBounds&&) = default;
RegionCaptureBounds::RegionCaptureBounds(const RegionCaptureBounds&) = default;
RegionCaptureBounds& RegionCaptureBounds::operator=(RegionCaptureBounds&&) =
    default;
RegionCaptureBounds& RegionCaptureBounds::operator=(
    const RegionCaptureBounds&) = default;
RegionCaptureBounds::~RegionCaptureBounds() = default;

// static
const RegionCaptureBounds& RegionCaptureBounds::Empty() {
  static base::NoDestructor<RegionCaptureBounds> kEmpty;
  return *kEmpty;
}

void RegionCaptureBounds::Set(const RegionCaptureCropId& crop_id,
                              const gfx::Rect& region) {
  bounds_.insert_or_assign(crop_id, region);
}

void RegionCaptureBounds::Reset() {
  bounds_.clear();
}

bool RegionCaptureBounds::operator==(const RegionCaptureBounds& rhs) const {
  return bounds_ == rhs.bounds_;
}
bool RegionCaptureBounds::operator!=(const RegionCaptureBounds& rhs) const {
  return !(*this == rhs);
}

std::string RegionCaptureBounds::ToString() const {
  std::ostringstream ss;
  ss << "{";
  for (auto it = bounds_.begin(); it != bounds_.end(); ++it) {
    if (it != bounds_.begin()) {
      ss << ",";
    }
    ss << "{" << it->first.ToString() << ", " << it->second.ToString() << "}";
  }
  ss << "}";
  return ss.str();
}

}  // namespace viz
