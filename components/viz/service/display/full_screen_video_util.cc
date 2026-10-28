// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/full_screen_video_util.h"

#include <algorithm>

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/viz_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"

namespace viz {

gfx::RectF ContainedAndCenteredRect(const gfx::RectF& rect,
                                    const gfx::SizeF& target_size) {
  if (rect.IsEmpty()) {
    return gfx::RectF();
  }

  // Scale factor that makes `rect` fit inside `target_size`, touching at least
  // two opposite sides.
  const double rect_to_target_size_scale = std::min(
      target_size.width() / rect.width(), target_size.height() / rect.height());

  gfx::RectF scaled_and_centered_rect = gfx::RectF(rect.size());
  scaled_and_centered_rect.Scale(rect_to_target_size_scale);
  scaled_and_centered_rect.Offset(
      (target_size.width() - scaled_and_centered_rect.width()) / 2.0,
      (target_size.height() - scaled_and_centered_rect.height()) / 2.0);

  return scaled_and_centered_rect;
}

bool IsPossibleFullScreenLetterboxing(const DrawQuad* quad_below,
                                      const gfx::Rect& display_rect) {
  // Two cases are considered as possible fullscreen letterboxing:
  // 1. If the quad beneath the overlay quad is DrawQuad::Material::kSolidColor
  // with black, and it touches two sides of the screen, while starting at
  // display origin (0, 0).
  // 2. If the quad beneath the overlay quad is
  // DrawQuad::Material::kTiledContent, and it touches two sides of the screen,
  // while starting at display origin (0, 0).
  // For YouTube with F11 page fullscreen mode, the kTiledContent beneath the
  // overlay does not touch the right edge due to the existing of a scrolling
  // bar.
  if (quad_below) {
    if (quad_below->material == DrawQuad::Material::kTiledContent ||
        (quad_below->material == DrawQuad::Material::kSolidColor &&
         SolidColorDrawQuad::MaterialCast(quad_below)->color ==
             SkColors::kBlack)) {
      gfx::RectF beneath_rect = ClippedQuadRectangleF(quad_below);
      return (beneath_rect.origin() == gfx::PointF(display_rect.origin()) &&
              (beneath_rect.width() == display_rect.width() ||
               beneath_rect.height() == display_rect.height()));
    }
  }

  return false;
}

std::optional<gfx::RectF> GetFullScreenVideoRect(
    const gfx::RectF& target_rect,
    const std::optional<gfx::Rect>& clip_rect,
    const gfx::Rect& output_rect,
    float tolerance) {
  if (target_rect.IsEmpty()) {
    return std::nullopt;
  }

  // The ideal rect is `target_rect` scaled to fit and centered inside the full
  // screen render pass output rect.
  gfx::RectF ideal_full_screen_rect =
      ContainedAndCenteredRect(target_rect, gfx::SizeF(output_rect.size()));

  if (ideal_full_screen_rect.ApproximatelyEqual(
          gfx::RectF(output_rect), kFullScreenVideoTightTolerance,
          kFullScreenVideoTightTolerance)) {
    // Snap the "ideal" rect to the monitor rect if we are close enough.
    //
    // One of cases that `kFullScreenVideoTightTolerance` addresses is with
    // non-integer device scale factors, which can result in a root frame size
    // that is one rounded up to be one pixel larger than the monitor size.
    ideal_full_screen_rect = gfx::RectF(output_rect);
  }

  const bool is_not_clipped =
      !clip_rect || gfx::RectF(clip_rect.value())
                        .ApproximatelyEqual(ideal_full_screen_rect,
                                            kFullScreenVideoTightTolerance,
                                            kFullScreenVideoTightTolerance);
  if (!is_not_clipped) {
    // If the video is clipped (and the clip rect is not equal to the target
    // rect), then it's not considered full screen.
    //
    // Clip rects that are larger than the video are conservatively not
    // allowed, but this may not be a requirement.
    return std::nullopt;
  }

  if (!target_rect.ApproximatelyEqual(ideal_full_screen_rect, tolerance,
                                      tolerance)) {
    return std::nullopt;
  }

  return ideal_full_screen_rect;
}

}  // namespace viz
