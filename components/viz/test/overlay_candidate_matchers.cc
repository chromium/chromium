// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/overlay_candidate_matchers.h"

#include "base/trace_event/traced_value.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"

namespace viz {

void PrintTo(const OverlayCandidate& candidate, std::ostream* os) {
  base::trace_event::TracedValueJSON value;
  if (candidate.plane_z_order) {
    value.SetInteger("plane_z_order", candidate.plane_z_order);
  }
  if (candidate.rpdq) {
    value.SetInteger("rpdq.render_pass_id",
                     candidate.rpdq->render_pass_id.value());
  }
  if (candidate.resource_id) {
    value.SetInteger("resource_id", candidate.resource_id.value());
  }
  if (candidate.is_solid_color) {
    value.SetBoolean("is_solid_color", true);
  }
  if (candidate.color) {
    auto color = value.BeginDictionaryScoped("color");
    value.SetDouble("r", candidate.color->fR);
    value.SetDouble("g", candidate.color->fG);
    value.SetDouble("b", candidate.color->fB);
    value.SetDouble("a", candidate.color->fA);
  }
  *os << value.ToJSON();
}

namespace test {

bool PlaneZOrderAscendingComparator::operator()(
    const OverlayCandidate& a,
    const OverlayCandidate& b) const {
  return a.plane_z_order < b.plane_z_order;
}

testing::Matcher<const OverlayCandidate&> IsRenderPassOverlay(
    AggregatedRenderPassId id) {
  return testing::AllOf(
      testing::Field("rpdq", &OverlayCandidate::rpdq, testing::NotNull()),
      testing::Field(
          "rpdq", &OverlayCandidate::rpdq,
          testing::Pointee(testing::Field(
              "render_pass_id", &AggregatedRenderPassDrawQuad::render_pass_id,
              testing::Eq(id)))));
}

testing::Matcher<const OverlayCandidate&> IsSolidColorOverlay(SkColor4f color) {
  return testing::AllOf(
      testing::Truly([](const OverlayCandidate& candidate) {
        // We can't use `testing::Eq` here since we can't get the address of the
        // bitfield `is_solid_color`.
        return candidate.is_solid_color;
      }),
      testing::Field("color", &OverlayCandidate::color, testing::Eq(color)));
}

testing::Matcher<const OverlayCandidate&> OverlayHasResource(
    ResourceId resource_id) {
  return testing::Field("resource_id", &OverlayCandidate::resource_id,
                        testing::Eq(resource_id));
}

testing::Matcher<const OverlayCandidate&> OverlayHasClip(
    std::optional<gfx::Rect> clip_rect) {
  return testing::Field("clip_rect", &OverlayCandidate::clip_rect,
                        testing::Eq(clip_rect));
}

testing::Matcher<const OverlayCandidate&> OverlayHasRoundedCorners(
    gfx::RRectF rounded_corners) {
  return testing::Field("rounded_corners", &OverlayCandidate::rounded_corners,
                        testing::Eq(rounded_corners));
}

}  // namespace test

}  // namespace viz
