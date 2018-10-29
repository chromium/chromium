// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_fullscreen.h"

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace viz {

OverlayStrategyFullscreen::OverlayStrategyFullscreen(
    OverlayCandidateValidator* capability_checker)
    : capability_checker_(capability_checker) {
  DCHECK(capability_checker);
}

OverlayStrategyFullscreen::~OverlayStrategyFullscreen() {}

bool OverlayStrategyFullscreen::Attempt(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  QuadList* quad_list = &render_pass->quad_list;
  // First quad of quad_list is the top most quad.
  auto front = quad_list->begin();
  while (front != quad_list->end()) {
    if (!OverlayCandidate::IsInvisibleQuad(*front))
      break;
    ++front;
  }

  if (front == quad_list->end())
    return false;

  const DrawQuad* quad = *front;
  if (quad->ShouldDrawWithBlending())
    return false;

  OverlayCandidate candidate;
  if (!OverlayCandidate::FromDrawQuad(resource_provider, output_color_matrix,
                                      quad, &candidate)) {
    return false;
  }

  if (!candidate.display_rect.origin().IsOrigin() ||
      gfx::ToRoundedSize(candidate.display_rect.size()) !=
          render_pass->output_rect.size()) {
    return false;
  }
  candidate.is_opaque = true;
  candidate.plane_z_order = 0;
  OverlayCandidateList new_candidate_list;
  new_candidate_list.push_back(candidate);
  capability_checker_->CheckOverlaySupport(&new_candidate_list);
  if (!new_candidate_list.front().overlay_handled)
    return false;

  candidate_list->swap(new_candidate_list);

  render_pass->quad_list = QuadList();  // Remove all the quads
  return true;
}

OverlayProcessor::StrategyType OverlayStrategyFullscreen::GetUMAEnum() const {
  return OverlayProcessor::StrategyType::kFullscreen;
}

}  // namespace viz
