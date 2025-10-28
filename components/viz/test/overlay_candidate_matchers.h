// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_OVERLAY_CANDIDATE_MATCHERS_H_
#define COMPONENTS_VIZ_TEST_OVERLAY_CANDIDATE_MATCHERS_H_

#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/overlay_layer_id.h"

namespace gfx {
void PrintTo(const OverlayLayerId& layer_id, std::ostream* os);
}

namespace viz {

// Provides human readable quad material names for gtest/gmock.
void PrintTo(const OverlayCandidate& candidate, std::ostream* os);

namespace test {

// Matches a render pass overlay with matching `render_pass_id`.
testing::Matcher<const OverlayCandidate&> IsRenderPassOverlay(
    AggregatedRenderPassId id);

// Matches a solid color overlay with matching `color`.
testing::Matcher<const OverlayCandidate&> IsSolidColorOverlay(SkColor4f color);

// Matches am overlay with matching `resource_id`.
testing::Matcher<const OverlayCandidate&> OverlayHasResource(
    ResourceId resource_id);

testing::Matcher<const OverlayCandidate&> OverlayHasClip(
    std::optional<gfx::Rect> clip_rect);

testing::Matcher<const OverlayCandidate&> OverlayHasRoundedCorners(
    gfx::RRectF rounded_corners);

testing::Matcher<const OverlayCandidate&> OverlayIsFullScreen();

testing::Matcher<const OverlayCandidate&> OverlayTargetRectIs(
    const gfx::RectF& expected);

// Return the count of non-primary-plane overlays in `candidate_list`.
size_t NumOverlaysExcludingPrimaryPlane(
    const OverlayCandidateList& candidate_list);

// Matches an `OverlayCandidateList` with a primary plane overlay that matches
// `is_opaque`.
MATCHER_P(HasPrimaryPlaneWithOpaqueness, is_opaque, "") {
  if (auto it = std::ranges::find_if(
          arg, [](const auto& overlay) { return overlay.is_root_render_pass; });
      it != arg.end()) {
    return it->is_opaque == is_opaque;
  } else {
    return false;
  }
}

}  // namespace test

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_OVERLAY_CANDIDATE_MATCHERS_H_
