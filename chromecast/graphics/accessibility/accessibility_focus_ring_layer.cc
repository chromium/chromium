// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#include "chromecast/graphics/accessibility/accessibility_focus_ring_layer.h"

#include <algorithm>

#include "base/bind.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromecast {

namespace {

// The number of pixels in the color gradient that fades to transparent.
const int kGradientWidth = 6;

// The color of the focus ring. In the future this might be a parameter.
const int kFocusRingColorRed = 247;
const int kFocusRingColorGreen = 152;
const int kFocusRingColorBlue = 58;

int sign(int x) {
  return ((x > 0) ? 1 : (x == 0) ? 0 : -1);
}

SkPath MakePath(const AccessibilityFocusRing& input_ring,
                int outset,
                const gfx::Vector2d& offset) {
  AccessibilityFocusRing ring = input_ring;

  for (int i = 0; i < 36; i++) {
    gfx::Point p = input_ring.points[i];
    gfx::Point prev;
    gfx::Point next;

    int prev_index = i;
    do {
      prev_index = (prev_index + 35) % 36;
      prev = input_ring.points[prev_index];
    } while (prev.x() == p.x() && prev.y() == p.y() && prev_index != i);

    int next_index = i;
    do {
      next_index = (next_index + 1) % 36;
      next = input_ring.points[next_index];
    } while (next.x() == p.x() && next.y() == p.y() && next_index != i);

    gfx::Point delta0 =
        gfx::Point(sign(p.x() - prev.x()), sign(p.y() - prev.y()));
    gfx::Point delta1 =
        gfx::Point(sign(next.x() - p.x()), sign(next.y() - p.y()));

    if (delta0.x() == delta1.x() && delta0.y() == delta1.y()) {
      ring.points[i] =
          gfx::Point(input_ring.points[i].x() + outset * delta0.y(),
                     input_ring.points[i].y() - outset * delta0.x());
    } else {
      ring.points[i] = gfx::Point(
          input_ring.points[i].x() + ((i + 13) % 36 >= 18 ? outset : -outset),
          input_ring.points[i].y() + ((i + 4) % 36 >= 18 ? outset : -outset));
    }
  }

  SkPath path;
  gfx::Point start = ring.points[0] - offset;
  path.moveTo(SkIntToScalar(start.x()), SkIntToScalar(start.y()));
  for (int i = 0; i < 12; i++) {
    int index0 = ((3 * i) + 1) % 36;
    int index1 = ((3 * i) + 2) % 36;
    int index2 = ((3 * i) + 3) % 36;
    gfx::Point p0 = ring.points[index0] - offset;
    gfx::Point p1 = ring.points[index1] - offset;
    gfx::Point p2 = ring.points[index2] - offset;
    path.lineTo(SkIntToScalar(p0.x()), SkIntToScalar(p0.y()));
    path.quadTo(SkIntToScalar(p1.x()), SkIntToScalar(p1.y()),
                SkIntToScalar(p2.x()), SkIntToScalar(p2.y()));
  }

  return path;
}

}  // namespace

AccessibilityFocusRingLayer::AccessibilityFocusRingLayer(
    aura::Window* root_window,
    AccessibilityLayerDelegate* delegate)
    : FocusRingLayer(root_window, delegate) {}

AccessibilityFocusRingLayer::~AccessibilityFocusRingLayer() = default;

void AccessibilityFocusRingLayer::Set(const AccessibilityFocusRing& ring) {
  ring_ = ring;

  gfx::Rect bounds = ring.GetBounds();
  int inset = kGradientWidth;
  bounds.Inset(-inset, -inset, -inset, -inset);

  DCHECK(root_window());
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  ::wm::ConvertRectFromScreen(root_window(), &bounds);
  CreateOrUpdateLayer(root_window(), "AccessibilityFocusRing", bounds);

  gfx::Rect wbounds = root_window()->bounds();
  wbounds.Inset(inset / 2, inset / 2, inset / 2, inset / 2);
  int top = wbounds.x();
  int left = wbounds.y();
  int bottom = top + wbounds.height();
  int right = left + wbounds.width();

  for (int i = 0; i < 36; i++) {
    gfx::Point& p = ring_.points[i];
    p.set_x(std::max(p.x(), left));
    p.set_x(std::min(p.x(), right));
    p.set_y(std::max(p.y(), top));
    p.set_y(std::min(p.y(), bottom));
  }
}

void AccessibilityFocusRingLayer::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2);

  SkColor base_color =
      has_custom_color()
          ? custom_color()
          : SkColorSetARGB(255, kFocusRingColorRed, kFocusRingColorGreen,
                           kFocusRingColorBlue);

  SkPath path;
  gfx::Vector2d offset = layer()->bounds().OffsetFromOrigin();
  const int w = kGradientWidth;
  for (int i = 0; i < w; ++i) {
    flags.setColor(SkColorSetA(base_color, 255 * (w - i) * (w - i) / (w * w)));
    path = MakePath(ring_, i, offset);
    recorder.canvas()->DrawPath(path, flags);
  }
}

}  // namespace chromecast
