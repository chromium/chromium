// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DELEGATED_INK_METADATA_H_
#define COMPONENTS_VIZ_COMMON_DELEGATED_INK_METADATA_H_

#include <string>

#include "base/time/time.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {

// This class stores all the metadata that is gathered when the WebAPI
// updateInkTrailStartPoint is called. This metadata flows from blink,
// through cc, and into viz in order to produce a delegated ink trail on the
// end of what was already rendered.
//
// Explainer for the feature:
// https://github.com/WICG/ink-enhancement/blob/master/README.md
class VIZ_COMMON_EXPORT DelegatedInkMetadata {
 public:
  DelegatedInkMetadata() = default;
  DelegatedInkMetadata(const gfx::PointF& pt,
                       double diameter,
                       SkColor color,
                       base::TimeTicks timestamp,
                       const gfx::RectF& area,
                       bool hovering)
      : point_(pt),
        diameter_(diameter),
        color_(color),
        timestamp_(timestamp),
        presentation_area_(area),
        is_hovering_(hovering) {}
  DelegatedInkMetadata(const gfx::PointF& pt,
                       double diameter,
                       SkColor color,
                       base::TimeTicks timestamp,
                       const gfx::RectF& area,
                       base::TimeTicks frame_time,
                       bool hovering)
      : point_(pt),
        diameter_(diameter),
        color_(color),
        timestamp_(timestamp),
        presentation_area_(area),
        frame_time_(frame_time),
        is_hovering_(hovering) {}
  DelegatedInkMetadata(const DelegatedInkMetadata& other) = default;

  const gfx::PointF& point() const { return point_; }
  double diameter() const { return diameter_; }
  SkColor color() const { return color_; }
  base::TimeTicks timestamp() const { return timestamp_; }
  const gfx::RectF& presentation_area() const { return presentation_area_; }
  base::TimeTicks frame_time() const { return frame_time_; }
  bool is_hovering() const { return is_hovering_; }

  void set_frame_time(base::TimeTicks frame_time) { frame_time_ = frame_time; }

  std::string ToString() const;

 private:
  // Location of the pointerevent relative to the root frame.
  gfx::PointF point_;

  // Width of the trail, in physical pixels.
  double diameter_ = 0;

  // Color to draw the ink trail.
  SkColor color_ = 0;

  // Timestamp from the pointerevent for the ink point.
  base::TimeTicks timestamp_;

  // The rect to clip the ink trail to, defaults to the containing viewport.
  gfx::RectF presentation_area_;

  // Frame time of the layer tree that this metadata is on.
  base::TimeTicks frame_time_;

  // True if the left mouse button is up or if a stylus with hovering
  // capabilities is hovering over the screen when updateInkTrailStartPoint is
  // called.
  bool is_hovering_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DELEGATED_INK_METADATA_H_
