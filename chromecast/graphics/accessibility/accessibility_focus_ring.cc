// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#include "chromecast/graphics/accessibility/accessibility_focus_ring.h"

#include <stddef.h>


namespace chromecast {

// static
AccessibilityFocusRing AccessibilityFocusRing::CreateWithRect(
    const gfx::Rect& bounds,
    int margin) {
  // Compute the height of the top and bottom cap.
  int cap_height = std::min(bounds.height() / 2, margin * 2);

  gfx::Rect top(bounds.x(), bounds.y(), bounds.width(), cap_height);
  gfx::Rect bottom(bounds.x(), bounds.bottom() - cap_height, bounds.width(),
                   cap_height);
  gfx::Rect body(bounds.x(), top.bottom(), bounds.width(),
                 bottom.y() - top.bottom());

  return CreateWithParagraphShape(top, body, bottom, margin);
}

// static
AccessibilityFocusRing AccessibilityFocusRing::Interpolate(
    const AccessibilityFocusRing& r1,
    const AccessibilityFocusRing& r2,
    double fraction) {
  AccessibilityFocusRing dst;
  for (int i = 0; i < 36; ++i) {
    dst.points[i] = gfx::Point(
        r1.points[i].x() * (1 - fraction) + r2.points[i].x() * fraction,
        r1.points[i].y() * (1 - fraction) + r2.points[i].y() * fraction);
  }
  return dst;
}

// static
AccessibilityFocusRing AccessibilityFocusRing::CreateWithParagraphShape(
    const gfx::Rect& orig_top_line,
    const gfx::Rect& orig_body,
    const gfx::Rect& orig_bottom_line,
    int margin) {
  gfx::Rect top = orig_top_line;
  gfx::Rect middle = orig_body;
  gfx::Rect bottom = orig_bottom_line;

  int min_height = std::min(top.height(), bottom.height());
  margin = std::min(margin, min_height / 2);

  if (top.x() <= middle.x() + 2 * margin) {
    top.set_width(top.width() + top.x() - middle.x());
    top.set_x(middle.x());
  }
  if (top.right() >= middle.right() - 2 * margin) {
    top.set_width(middle.right() - top.x());
  }

  if (bottom.x() <= middle.x() + 2 * margin) {
    bottom.set_width(bottom.width() + bottom.x() - middle.x());
    bottom.set_x(middle.x());
  }
  if (bottom.right() >= middle.right() - 2 * margin) {
    bottom.set_width(middle.right() - bottom.x());
  }

  AccessibilityFocusRing ring;
  ring.points[0] = gfx::Point(top.x(), top.bottom() - margin);
  ring.points[1] = gfx::Point(top.x(), top.y() + margin);
  ring.points[2] = gfx::Point(top.x(), top.y());
  ring.points[3] = gfx::Point(top.x() + margin, top.y());
  ring.points[4] = gfx::Point(top.right() - margin, top.y());
  ring.points[5] = gfx::Point(top.right(), top.y());
  ring.points[6] = gfx::Point(top.right(), top.y() + margin);
  ring.points[7] = gfx::Point(top.right(), top.bottom() - margin);
  ring.points[8] = gfx::Point(top.right(), top.bottom());
  if (top.right() < middle.right()) {
    ring.points[9] = gfx::Point(top.right() + margin, middle.y());
    ring.points[10] = gfx::Point(middle.right() - margin, middle.y());
  } else {
    ring.points[9] = gfx::Point(top.right(), middle.y());
    ring.points[10] = gfx::Point(middle.right(), middle.y());
  }
  ring.points[11] = gfx::Point(middle.right(), middle.y());
  ring.points[12] = gfx::Point(middle.right(), middle.y() + margin);
  ring.points[13] = gfx::Point(middle.right(), middle.bottom() - margin);
  ring.points[14] = gfx::Point(middle.right(), middle.bottom());
  if (bottom.right() < middle.right()) {
    ring.points[15] = gfx::Point(middle.right() - margin, bottom.y());
    ring.points[16] = gfx::Point(bottom.right() + margin, bottom.y());
  } else {
    ring.points[15] = gfx::Point(middle.right(), bottom.y());
    ring.points[16] = gfx::Point(bottom.right(), bottom.y());
  }
  ring.points[17] = gfx::Point(bottom.right(), bottom.y());
  ring.points[18] = gfx::Point(bottom.right(), bottom.y() + margin);
  ring.points[19] = gfx::Point(bottom.right(), bottom.bottom() - margin);
  ring.points[20] = gfx::Point(bottom.right(), bottom.bottom());
  ring.points[21] = gfx::Point(bottom.right() - margin, bottom.bottom());
  ring.points[22] = gfx::Point(bottom.x() + margin, bottom.bottom());
  ring.points[23] = gfx::Point(bottom.x(), bottom.bottom());
  ring.points[24] = gfx::Point(bottom.x(), bottom.bottom() - margin);
  ring.points[25] = gfx::Point(bottom.x(), bottom.y() + margin);
  ring.points[26] = gfx::Point(bottom.x(), bottom.y());
  if (bottom.x() > middle.x()) {
    ring.points[27] = gfx::Point(bottom.x() - margin, bottom.y());
    ring.points[28] = gfx::Point(middle.x() + margin, middle.bottom());
  } else {
    ring.points[27] = gfx::Point(bottom.x(), bottom.y());
    ring.points[28] = gfx::Point(middle.x(), middle.bottom());
  }
  ring.points[29] = gfx::Point(middle.x(), middle.bottom());
  ring.points[30] = gfx::Point(middle.x(), middle.bottom() - margin);
  ring.points[31] = gfx::Point(middle.x(), middle.y() + margin);
  ring.points[32] = gfx::Point(middle.x(), middle.y());
  if (top.x() > middle.x()) {
    ring.points[33] = gfx::Point(middle.x() + margin, middle.y());
    ring.points[34] = gfx::Point(top.x() - margin, top.bottom());
  } else {
    ring.points[33] = gfx::Point(middle.x(), middle.y());
    ring.points[34] = gfx::Point(top.x(), top.bottom());
  }
  ring.points[35] = gfx::Point(top.x(), top.bottom());

  return ring;
}

gfx::Rect AccessibilityFocusRing::GetBounds() const {
  gfx::Point top_left = points[0];
  gfx::Point bottom_right = points[0];
  for (size_t i = 1; i < 36; ++i) {
    top_left.SetToMin(points[i]);
    bottom_right.SetToMax(points[i]);
  }
  return gfx::Rect(top_left, gfx::Size(bottom_right.x() - top_left.x(),
                                       bottom_right.y() - top_left.y()));
}

}  // namespace chromecast
