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

namespace viz {

// Provides human readable quad material names for gtest/gmock.
void PrintTo(const OverlayCandidate& candidate, std::ostream* os);

namespace test {

// Comparator for sorting algorithms that places candidates in ascending order
// using their z-order as the sort key.
struct PlaneZOrderAscendingComparator {
  bool operator()(const OverlayCandidate& a, const OverlayCandidate& b) const;
};

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

}  // namespace test

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_OVERLAY_CANDIDATE_MATCHERS_H_
