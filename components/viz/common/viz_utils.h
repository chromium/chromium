// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_
#define COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_

#include <string>

#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/viz_common_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace viz {

#if BUILDFLAG(IS_ANDROID)
VIZ_COMMON_EXPORT bool AlwaysUseWideColorGamut();
#endif

class CopyOutputRequest;
class RenderPassDrawQuadInternal;

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
// pixel-moving foreground filter. The returned bounds are in the quad's target
// coordinate space.
VIZ_COMMON_EXPORT gfx::Rect GetTargetExpandedRectForPixelMovingFilters(
    const RenderPassDrawQuadInternal& rpdq,
    const cc::FilterOperations& filters);

// The expanded area that will be changed by a render pass draw quad with a
// pixel-moving foreground filter. The returned bounds are in the quad's
// original coordinate space.
VIZ_COMMON_EXPORT gfx::Rect GetExpandedRectForPixelMovingFilters(
    const RenderPassDrawQuadInternal& rpdq,
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
VIZ_COMMON_EXPORT void SetCopyOutputRequestResultSize(
    CopyOutputRequest* request,
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    const gfx::Size& surface_size_in_pixels);

}  // namespace viz

#define VIZ_HIT_PATH(path_name)                                         \
  do {                                                                  \
    static bool init = false;                                           \
    if (!init) {                                                        \
      std::string name =                                                \
          "Compositing.Display.VizCodePath." + std::string(#path_name); \
      UMA_HISTOGRAM_BOOLEAN(name, true);                                \
      init = true;                                                      \
    }                                                                   \
  } while (0)

#endif  // COMPONENTS_VIZ_COMMON_VIZ_UTILS_H_
