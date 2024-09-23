// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_
#define COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_

#include "base/timer/elapsed_timer.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/viz_common_export.h"

#include "build/build_config.h"

namespace gfx {
class Rect;
class RRectF;
class QuadF;
}  // namespace gfx

namespace viz {

#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool PreferRGB565ResourcesForDisplay();
VIZ_COMMON_EXPORT bool AlwaysUseWideColorGamut();
#endif

class CopyOutputRequest;

// This takes a gfx::Rect and a clip region quad in the same space,
// and returns a quad with the same proportions in the space -0.5->0.5.
VIZ_COMMON_EXPORT bool GetScaledRegion(const gfx::Rect& rect,
                                       const gfx::QuadF* clip,
                                       gfx::QuadF* scaled_region);
// This takes a rounded rect and a rect that it lives in, and returns an
// equivalent rounded rect in the space -0.5->0.5.
VIZ_COMMON_EXPORT bool GetScaledRRectF(const gfx::Rect& space,
                                       const gfx::RRectF& rect,
                                       gfx::RRectF* scaled_rect);

// Returns File Descriptor (FD) stats for current process.
// Rendering resources can consume FDs. This this function can be used to
// determine if the process is low on FDs or find an FD leak.
VIZ_COMMON_EXPORT bool GatherFDStats(base::TimeDelta* delta_time_taken,
                                     int* fd_max,
                                     int* active_fd_count,
                                     int* rlim_cur);

// Returns the smallest rectangle in target space that contains the quad.
VIZ_COMMON_EXPORT gfx::Rect ClippedQuadRectangle(const DrawQuad* quad);
VIZ_COMMON_EXPORT gfx::RectF ClippedQuadRectangleF(const DrawQuad* quad);

// The expanded area that will be changed by a render pass draw quad with a
// pixel-moving foreground filter.
VIZ_COMMON_EXPORT gfx::Rect GetExpandedRectWithPixelMovingForegroundFilter(
    const DrawQuad& rpdq,
    const cc::FilterOperations& filters);

// This transforms a rect from the view transition content surface/render_pass
// space to the shared element quad space.
VIZ_COMMON_EXPORT gfx::Transform GetViewTransitionTransform(
    gfx::Rect shared_element_quad,
    gfx::Rect view_transition_content_output);

// Returns true if the quad's visible rect bounds overlaps with at least one
// of the rounded corners bounding rects.
VIZ_COMMON_EXPORT bool QuadRoundedCornersBoundsIntersects(
    const DrawQuad* quad,
    const gfx::RectF& target_quad);

// Customizes the output sizes of a `CopyOutputRequest`.
VIZ_COMMON_EXPORT void SetCopyOutoutRequestResultSize(
    CopyOutputRequest* request,
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    const gfx::Size& surface_size_in_pixels);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_
