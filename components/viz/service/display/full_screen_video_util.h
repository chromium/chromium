// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_FULL_SCREEN_VIDEO_UTIL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_FULL_SCREEN_VIDEO_UTIL_H_

#include <optional>

#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace viz {

class DrawQuad;

// Helpers to detect a full screen (possibly letterboxed) video, shared by
// platforms that treat full screen video specially.

// The tolerance, in pixels, for a video to be considered placed exactly at its
// full screen position. This allows for a browser or video renderer that
// doesn't supply a perfectly placed video quad, e.g. due to rounding with
// non-integer device scale factors.
inline constexpr float kFullScreenVideoTightTolerance = 1.0f;

// Centers and scales `rect` inside `target_size` to the largest size such that
// it maintains its aspect ratio and is fully contained by `target_size`.
// Returns an empty rect if `rect` is empty.
VIZ_SERVICE_EXPORT gfx::RectF ContainedAndCenteredRect(
    const gfx::RectF& rect,
    const gfx::SizeF& target_size);

// Returns true if the `quad_below` meets the criteria to allow the quad above
// it to be considered a possibly letterboxed full screen video.
// - `quad_below` must be the quad directly below the quad being checked. If
//   null, indicates there is nothing below the quad being checked.
// - `display_rect` is ideally the monitor rect, but is approximated with the
//   root render pass output rect.
// See implementation for details.
VIZ_SERVICE_EXPORT bool IsPossibleFullScreenLetterboxing(
    const DrawQuad* quad_below,
    const gfx::Rect& display_rect);

// Returns the full screen rect for a video displayed at `target_rect` (in the
// root render pass's target space), clipped by `clip_rect`, if the video is
// scaled to fit and centered inside `output_rect`: i.e. it is within
// `tolerance` of its ideal full screen position and is not clipped (or clipped
// exactly to the ideal rect within `kFullScreenVideoTightTolerance`). Returns
// nullopt otherwise, or if `target_rect` is empty. The returned rect is the
// ideal full screen rect, snapped to `output_rect` if close enough.
VIZ_SERVICE_EXPORT std::optional<gfx::RectF> GetFullScreenVideoRect(
    const gfx::RectF& target_rect,
    const std::optional<gfx::Rect>& clip_rect,
    const gfx::Rect& output_rect,
    float tolerance);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_FULL_SCREEN_VIDEO_UTIL_H_
