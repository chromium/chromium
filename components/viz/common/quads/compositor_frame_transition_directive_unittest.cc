// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include "components/viz/common/transition_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {
namespace {

using Type = CompositorFrameTransitionDirective::Type;

TEST(CompositorFrameTransitionDirective, GettersReflectParameters) {
  blink::ViewTransitionToken transition_token;
  auto save_directive = CompositorFrameTransitionDirective::CreateSave(
      transition_token, /*maybe_cross_frame_sink=*/false, 1u, {}, {},
      /*delay_layer_tree_view_deletion=*/false);

  EXPECT_EQ(1u, save_directive.sequence_id());
  EXPECT_EQ(Type::kSave, save_directive.type());
  EXPECT_EQ(transition_token, save_directive.transition_token());
  EXPECT_FALSE(save_directive.maybe_cross_frame_sink());
  EXPECT_FALSE(save_directive.delay_layer_tree_view_deletion());

  auto animate_directive = CompositorFrameTransitionDirective::CreateAnimate(
      transition_token, /*maybe_cross_frame_sink=*/true, 2,
      /*delay_layer_tree_view_deletion=*/false);

  EXPECT_EQ(2u, animate_directive.sequence_id());
  EXPECT_EQ(Type::kAnimateRenderer, animate_directive.type());
  EXPECT_EQ(transition_token, animate_directive.transition_token());
  EXPECT_TRUE(animate_directive.maybe_cross_frame_sink());
  EXPECT_FALSE(animate_directive.delay_layer_tree_view_deletion());
}

TEST(CompositorFrameTransitionDirective, DelayLayerTreeViewDeletion) {
  blink::ViewTransitionToken transition_token;
  auto save_directive = CompositorFrameTransitionDirective::CreateSave(
      transition_token, /*maybe_cross_frame_sink=*/false, 1u, {}, {},
      /*delay_layer_tree_view_deletion=*/true);
  EXPECT_TRUE(save_directive.delay_layer_tree_view_deletion());
}

TEST(TransitionUtilsTest, ComputePixelAlignmentOffset) {
  // Pure translation with subpixel offset.
  {
    gfx::Transform transform = gfx::Transform::MakeTranslation(10.25f, 20.75f);
    gfx::Vector2dF offset =
        TransitionUtils::ComputePixelAlignmentOffset(transform);
    EXPECT_NEAR(offset.x(), 0.25f, 1e-5f);
    EXPECT_NEAR(offset.y(), 0.75f, 1e-5f);
  }

  // Integer translation -> zero subpixel offset.
  {
    gfx::Transform transform = gfx::Transform::MakeTranslation(10.0f, 20.0f);
    gfx::Vector2dF offset =
        TransitionUtils::ComputePixelAlignmentOffset(transform);
    EXPECT_NEAR(offset.x(), 0.0f, 1e-5f);
    EXPECT_NEAR(offset.y(), 0.0f, 1e-5f);
  }

  // Negative translation with subpixel offset.
  {
    // floor(-10.25) = -11, so -10.25 - (-11) = 0.75.
    gfx::Transform transform =
        gfx::Transform::MakeTranslation(-10.25f, -20.75f);
    gfx::Vector2dF offset =
        TransitionUtils::ComputePixelAlignmentOffset(transform);
    EXPECT_NEAR(offset.x(), 0.75f, 1e-5f);
    EXPECT_NEAR(offset.y(), 0.25f, 1e-5f);
  }

  // Scale + translation.
  {
    gfx::Transform transform = gfx::Transform::MakeScale(2.0f);
    transform.PostTranslate(5.3f, 7.8f);
    gfx::Vector2dF offset =
        TransitionUtils::ComputePixelAlignmentOffset(transform);
    EXPECT_NEAR(offset.x(), 0.3f, 1e-5f);
    EXPECT_NEAR(offset.y(), 0.8f, 1e-5f);
  }

  // Complex transform (e.g. rotation) -> returns zero offset.
  {
    gfx::Transform transform;
    transform.RotateAboutZAxis(45.0f);
    transform.PostTranslate(10.5f, 20.5f);
    gfx::Vector2dF offset =
        TransitionUtils::ComputePixelAlignmentOffset(transform);
    EXPECT_TRUE(offset.IsZero());
  }
}

}  // namespace
}  // namespace viz
