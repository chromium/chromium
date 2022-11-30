// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_REGION_CAPTURE_BOUNDS_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_REGION_CAPTURE_BOUNDS_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "base/token.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

using RegionCaptureCropId = base::Token;

// RegionCaptureBounds maps precisely to the same-named mojom class, and is
// used for passing in region capture crop ids mapped to a gfx::Rect
// representing the region of the viewport that should be cropped to for
// tab capture.
// See the design document at: https://tinyurl.com/region-capture.
class VIZ_COMMON_EXPORT RegionCaptureBounds {
 public:
  RegionCaptureBounds();
  explicit RegionCaptureBounds(
      base::flat_map<RegionCaptureCropId, gfx::Rect> bounds);
  RegionCaptureBounds(RegionCaptureBounds&&);
  RegionCaptureBounds(const RegionCaptureBounds&);
  RegionCaptureBounds& operator=(RegionCaptureBounds&&);
  RegionCaptureBounds& operator=(const RegionCaptureBounds&);
  ~RegionCaptureBounds();

  // Returns a reference to a global empty RegionCaptureBounds. This should only
  // be used for functions that need to return a reference to a
  // RegionCaptureBounds, not instead of the default constructor.
  static const RegionCaptureBounds& Empty();

  // We currently only support a single set of bounds for a given crop id.
  // Multiple calls with the same crop id will update the bounds.
  void Set(const RegionCaptureCropId& crop_id, const gfx::Rect& bounds);

  bool IsEmpty() const { return bounds_.empty(); }

  // Sets the bounds to empty.
  void Reset();

  const base::flat_map<RegionCaptureCropId, gfx::Rect>& bounds() const {
    return bounds_;
  }

  bool operator==(const RegionCaptureBounds& rhs) const;
  bool operator!=(const RegionCaptureBounds& rhs) const;
  std::string ToString() const;

 private:
  base::flat_map<RegionCaptureCropId, gfx::Rect> bounds_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_REGION_CAPTURE_BOUNDS_H_
