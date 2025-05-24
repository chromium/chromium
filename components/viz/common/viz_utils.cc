// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/viz_utils.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/math_util.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/render_pass_draw_quad_internal.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"

#if BUILDFLAG(IS_ANDROID)
#include <array>
#include <string>

#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include <poll.h>
#include <sys/resource.h>
#endif

namespace viz {

#if BUILDFLAG(IS_ANDROID)
bool PreferRGB565ResourcesForDisplay() {
  return base::SysInfo::AmountOfPhysicalMemoryMB() <= 512;
}
#endif

#if BUILDFLAG(IS_ANDROID)
bool AlwaysUseWideColorGamut() {
  // Full stack integration tests draw in sRGB and expect to read back in sRGB.
  // WideColorGamut causes pixels to be drawn in P3, but read back doesn't tell
  // us the color space. So disable WCG for tests.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char kDisableWCGForTest[] = "disable-wcg-for-test";
  if (command_line.HasSwitch(kDisableWCGForTest))
    return false;

  // As it takes some work to compute this, cache the result.
  static bool is_always_use_wide_color_gamut_enabled = [] {
    const std::string& current_model =
        base::android::BuildInfo::GetInstance()->model();
    const std::array<std::string, 2> enabled_models = {
        std::string{"Pixel 4"}, std::string{"Pixel 4 XL"}};
    for (const std::string& model : enabled_models) {
      if (model == current_model) {
        return true;
      }
    }

    return false;
  }();

  return is_always_use_wide_color_gamut_enabled;
}
#endif

bool GatherFDStats(base::TimeDelta* delta_time_taken,
                   int* fd_max,
                   int* active_fd_count,
                   int* rlim_cur) {
#if !BUILDFLAG(IS_POSIX)
  return false;
#else   // BUILDFLAG(IS_POSIX)
  // https://stackoverflow.com/questions/7976769/
  // getting-count-of-current-used-file-descriptors-from-c-code
  base::ElapsedTimer timer;
  rlimit limit_data;
  getrlimit(RLIMIT_NOFILE, &limit_data);
  std::vector<pollfd> poll_data;
  constexpr int kMaxNumFDTested = 1 << 16;
  // |rlim_cur| is the soft max but is likely the value we can rely on instead
  // of the real max.
  *rlim_cur = static_cast<int>(limit_data.rlim_cur);
  *fd_max = std::max(1, std::min(*rlim_cur, kMaxNumFDTested));
  poll_data.resize(*fd_max);
  for (size_t i = 0; i < poll_data.size(); i++) {
    auto& each = poll_data[i];
    each.fd = static_cast<int>(i);
    each.events = 0;
    each.revents = 0;
  }

  poll(poll_data.data(), poll_data.size(), 0);
  *active_fd_count = 0;
  for (auto&& each : poll_data) {
    if (each.revents != POLLNVAL)
      (*active_fd_count)++;
  }
  *delta_time_taken = timer.Elapsed();
  return true;
#endif  // BUILDFLAG(IS_POSIX)
}
gfx::RectF ClippedQuadRectangleF(const DrawQuad* quad) {
  gfx::RectF quad_rect = cc::MathUtil::MapClippedRect(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->rect));
  if (quad->shared_quad_state->clip_rect)
    quad_rect.Intersect(gfx::RectF(*quad->shared_quad_state->clip_rect));
  return quad_rect;
}

gfx::Rect ClippedQuadRectangle(const DrawQuad* quad) {
  return gfx::ToEnclosingRect(ClippedQuadRectangleF(quad));
}

gfx::Rect GetTargetExpandedRectForPixelMovingFilters(
    const RenderPassDrawQuadInternal& rpdq,
    const cc::FilterOperations& filters) {
  const SharedQuadState* shared_quad_state = rpdq.shared_quad_state;
  gfx::Rect expanded_rect = GetExpandedRectForPixelMovingFilters(rpdq, filters);
  return cc::MathUtil::MapEnclosingClippedRect(
      shared_quad_state->quad_to_target_transform, expanded_rect);
}

gfx::Rect GetExpandedRectForPixelMovingFilters(
    const RenderPassDrawQuadInternal& rpdq,
    const cc::FilterOperations& filters) {
  if (!base::FeatureList::IsEnabled(features::kUseMapRectForPixelMovement)) {
    // ExpandRectForPixelMovement() has several problems that
    // GetExpandedRectForPixelMovingFilters() by calling MapRect instead.
    // 1. ExpandRectForPixelMovement's bounds propagation logic does not
    //    perfectly match how the underlying SkImageFilters compose together.
    // 2. It doesn't handle reference image filters, and assumes a fixed outset.
    // 3. It is unaware of the RPDQ's filters_origin and filters_scale, which
    //    define the matrix that must be passed into MapRect.
    //
    // When the MapRect feature is disabled, this preserves historic behavior
    // for callsites that used to call ExpandRectForPixelMovement directly, or
    // for callers of GetExpandedRectWithPixelMovingForegroundFilter (which is
    // now equivalent to GetTargetExpandedRectForPixelMovingFilters).
    return filters.ExpandRectForPixelMovement(rpdq.rect);
  }

  SkMatrix local_matrix =
      SkMatrix::Translate(rpdq.filters_origin.x(), rpdq.filters_origin.y());
  local_matrix.postScale(rpdq.filters_scale.x(), rpdq.filters_scale.y());

  return filters.MapRect(rpdq.visible_rect, local_matrix);
}

gfx::Transform GetViewTransitionTransform(
    gfx::Rect shared_element_quad,
    gfx::Rect view_transition_content_output) {
  gfx::Transform view_transition_transform;

  view_transition_transform.Translate(shared_element_quad.x(),
                                      shared_element_quad.y());

  view_transition_transform.Scale(
      shared_element_quad.width() /
          static_cast<SkScalar>(view_transition_content_output.width()),
      shared_element_quad.height() /
          static_cast<SkScalar>(view_transition_content_output.height()));

  view_transition_transform.Translate(-view_transition_content_output.x(),
                                      -view_transition_content_output.y());

  return view_transition_transform;
}

bool QuadRoundedCornersBoundsIntersects(const DrawQuad* quad,
                                        const gfx::RectF& target_quad) {
  const SharedQuadState* sqs = quad->shared_quad_state;
  const gfx::MaskFilterInfo& mask_filter_info = sqs->mask_filter_info;

  // There is no rounded corner set.
  if (!mask_filter_info.HasRoundedCorners()) {
    return false;
  }

  const gfx::RRectF& rounded_corner_bounds =
      mask_filter_info.rounded_corner_bounds();

  const gfx::RRectF::Corner corners[] = {
      gfx::RRectF::Corner::kUpperLeft, gfx::RRectF::Corner::kUpperRight,
      gfx::RRectF::Corner::kLowerRight, gfx::RRectF::Corner::kLowerLeft};
  for (auto c : corners) {
    if (rounded_corner_bounds.CornerBoundingRect(c).Intersects(target_quad)) {
      return true;
    }
  }
  return false;
}

void SetCopyOutoutRequestResultSize(CopyOutputRequest* request,
                                    const gfx::Rect& src_rect,
                                    const gfx::Size& output_size,
                                    const gfx::Size& surface_size_in_pixels) {
  CHECK(request);
  if (!src_rect.IsEmpty()) {
    request->set_area(src_rect);
  }
  if (output_size.IsEmpty()) {
    return;
  }
  // The CopyOutputRequest API does not allow fixing the output size. Instead
  // we have the set area and scale in such a way that it would result in the
  // desired output size.
  if (!request->has_area()) {
    request->set_area(gfx::Rect(surface_size_in_pixels));
  }
  request->set_result_selection(gfx::Rect(output_size));
  const gfx::Rect& area = request->area();
  // Viz would normally return an empty result for an empty area.
  // However, this guard here is still necessary to protect against setting
  // an illegal scaling ratio.
  if (area.IsEmpty()) {
    return;
  }
  request->SetScaleRatio(
      gfx::Vector2d(area.width(), area.height()),
      gfx::Vector2d(output_size.width(), output_size.height()));
}

}  // namespace viz
