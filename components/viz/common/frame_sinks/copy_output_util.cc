// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/copy_output_util.h"

#include <stdint.h>
#include <string>

#include "base/check_op.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {
namespace copy_output {

namespace {

// Returns the same values as std::ceil(t * numerator / denominator), but
// without introducing floating-point math and using 64-bit integer ops to avoid
// overflow.
int64_t CeilScale(int32_t t, int32_t numerator, int32_t denominator) {
  int64_t x = static_cast<int64_t>(t) * numerator;
  if (x > 0)
    x += denominator - 1;
  return x / denominator;
}

// Returns the same values as std::floor(t * numerator / denominator), but
// without introducing floating-point math and using 64-bit integer ops to avoid
// overflow.
int64_t FloorScale(int32_t t, int32_t numerator, int32_t denominator) {
  if (t < 0)
    return -CeilScale(-t, numerator, denominator);
  return (static_cast<int64_t>(t) * numerator) / denominator;
}

}  // namespace

std::string RenderPassGeometry::ToString() const {
  return base::StringPrintf(
      "sampling_bounds: %s, result_bounds: %s, result_selection: %s, "
      "readback_offset: %s",
      sampling_bounds.ToString().c_str(), result_bounds.ToString().c_str(),
      result_selection.ToString().c_str(), readback_offset.ToString().c_str());
}

gfx::Rect ComputeResultRect(const gfx::Rect& area,
                            const gfx::Vector2d& scale_from,
                            const gfx::Vector2d& scale_to) {
  DCHECK_GT(scale_from.x(), 0);
  DCHECK_GT(scale_from.y(), 0);
  DCHECK_GE(scale_to.x(), 0);
  DCHECK_GE(scale_to.y(), 0);

  const int64_t x = FloorScale(area.x(), scale_to.x(), scale_from.x());
  const int64_t y = FloorScale(area.y(), scale_to.y(), scale_from.y());
  const int64_t w =
      area.width() == 0
          ? 0
          : (CeilScale(area.right(), scale_to.x(), scale_from.x()) - x);
  const int64_t h =
      area.height() == 0
          ? 0
          : (CeilScale(area.bottom(), scale_to.y(), scale_from.y()) - y);

  // These constants define the "reasonable" range of result Rect coordinates.
  constexpr int kMaxOriginOffset = (1 << 24) - 1;  // Arbitrary, but practical.
  constexpr int kMaxDimension = (1 << 15) - 1;     // From media/base/limits.h.

  // If the result Rect is not "safely reasonable," return an empty Rect instead
  // to indicate to the client that scaling should not be attempted.
  if (x < -kMaxOriginOffset || x > kMaxOriginOffset || y < -kMaxOriginOffset ||
      y > kMaxOriginOffset || w <= 0 || w > kMaxDimension || h <= 0 ||
      h > kMaxDimension) {
    return gfx::Rect();
  }

  return gfx::Rect(static_cast<int>(x), static_cast<int>(y),
                   static_cast<int>(w), static_cast<int>(h));
}

RenderPassGeometry::RenderPassGeometry() = default;
RenderPassGeometry::~RenderPassGeometry() = default;

}  // namespace copy_output
}  // namespace viz
