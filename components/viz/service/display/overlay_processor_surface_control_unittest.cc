// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_surface_control.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace viz {

TEST(OverlayCandidateValidatorSurfaceControlTest, NoClipOrNegativeOffset) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(10.f, 10.f);
  candidate.uv_rect = gfx::RectF(1.f, 1.f);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayProcessorSurfaceControl processor;
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect, gfx::RectF(10.f, 10.f));
}

TEST(OverlayProcessorSurfaceControlTest, Clipped) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(10.f, 10.f);
  candidate.uv_rect = gfx::RectF(1.f, 1.f);
  candidate.clip_rect = gfx::Rect(2, 2, 5, 5);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayProcessorSurfaceControl processor;
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect,
                  gfx::RectF(2.f, 2.f, 5.f, 5.f));
  EXPECT_RECTF_EQ(candidates.at(0).uv_rect, gfx::RectF(0.2f, 0.2f, 0.5f, 0.5f));
}

TEST(OverlayProcessorSurfaceControlTest, NegativeOffset) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(-2.f, -4.f, 10.f, 10.f);
  candidate.uv_rect = gfx::RectF(0.5f, 0.5f);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayProcessorSurfaceControl processor;
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect,
                  gfx::RectF(0.f, 0.f, 8.f, 6.f));
  EXPECT_RECTF_EQ(candidates.at(0).uv_rect, gfx::RectF(0.1f, 0.2f, 0.4f, 0.3f));
}

TEST(OverlayProcessorSurfaceControlTest, ClipAndNegativeOffset) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(-5.0f, -5.0f, 10.0f, 10.0f);
  candidate.uv_rect = gfx::RectF(0.5f, 0.5f, 0.5f, 0.5f);
  candidate.clip_rect = gfx::Rect(5, 5);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayProcessorSurfaceControl processor;
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
  EXPECT_RECTF_EQ(candidates.at(0).display_rect,
                  gfx::RectF(0.f, 0.f, 5.f, 5.f));
  EXPECT_RECTF_EQ(candidates.at(0).uv_rect,
                  gfx::RectF(0.75f, 0.75f, 0.25f, 0.25f));
}

TEST(OverlayProcessorSurfaceControlTest, DisplayTransformOverlayVFlip) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(10, 10, 50, 100);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayProcessorSurfaceControl processor;
  processor.SetViewportSize(gfx::Size(100, 200));
  processor.SetDisplayTransformHint(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90);

  candidates.back().transform =
      gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90;
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_TRUE(candidates.back().overlay_handled);

  EXPECT_EQ(absl::get<gfx::OverlayTransform>(candidates.back().transform),
            gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL);
  EXPECT_RECTF_EQ(candidates.back().display_rect, gfx::RectF(10, 40, 100, 50));
}

TEST(OverlayProcessorSurfaceControlTest, DisplayTransformOverlay) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(10, 10, 50, 100);
  candidate.overlay_handled = false;

  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  OverlayProcessorSurfaceControl processor;
  processor.SetViewportSize(gfx::Size(100, 200));
  processor.SetDisplayTransformHint(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90);

  // First use a different transform than the display transform, the overlay is
  // rejected.
  candidates.back().transform = gfx::OVERLAY_TRANSFORM_NONE;
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_FALSE(candidates.back().overlay_handled);

  candidates.back().transform = gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_TRUE(candidates.back().overlay_handled);
  EXPECT_EQ(absl::get<gfx::OverlayTransform>(candidates.back().transform),
            gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_RECTF_EQ(candidates.back().display_rect, gfx::RectF(10, 40, 100, 50));
}

TEST(OverlayProcessorSurfaceControlTest, DisplayTransformOutputSurfaceOverlay) {
  OverlayProcessorInterface::OutputSurfaceOverlayPlane candidate;
  candidate.display_rect = gfx::RectF(100, 200);
  candidate.transform = gfx::OVERLAY_TRANSFORM_NONE;
  std::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>
      overlay_plane = candidate;

  OverlayProcessorSurfaceControl processor;
  processor.SetViewportSize(gfx::Size(100, 200));
  processor.SetDisplayTransformHint(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90);
  processor.AdjustOutputSurfaceOverlay(&overlay_plane);
  EXPECT_RECTF_EQ(overlay_plane.value().display_rect, gfx::RectF(200, 100));
  EXPECT_EQ(overlay_plane.value().transform,
            gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90);
}

TEST(OverlayCandidateValidatorTest, OverlayDamageRectForOutputSurface) {
  OverlayCandidate candidate;
  candidate.display_rect = gfx::RectF(10, 10, 50, 100);
  candidate.transform = gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
  candidate.overlay_handled = false;

  OverlayProcessorSurfaceControl processor;
  processor.SetViewportSize(gfx::Size(100, 200));
  processor.SetDisplayTransformHint(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90);

  OverlayCandidateList candidates;
  candidates.push_back(candidate);
  processor.CheckOverlaySupport(nullptr, &candidates);
  EXPECT_TRUE(candidates.back().overlay_handled);
  EXPECT_RECTF_EQ(candidates.back().display_rect, gfx::RectF(10, 40, 100, 50));
  EXPECT_EQ(processor.GetOverlayDamageRectForOutputSurface(candidates.back()),
            gfx::Rect(10, 10, 50, 100));
}

}  // namespace viz
