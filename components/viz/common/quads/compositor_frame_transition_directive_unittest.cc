// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using Type = CompositorFrameTransitionDirective::Type;

TEST(CompositorFrameTransitionDirective, GettersReflectParameters) {
  blink::ViewTransitionToken transition_token;
  auto save_directive = CompositorFrameTransitionDirective::CreateSave(
      transition_token, /*maybe_cross_frame_sink=*/false, 1u, {}, {});

  EXPECT_EQ(1u, save_directive.sequence_id());
  EXPECT_EQ(Type::kSave, save_directive.type());
  EXPECT_EQ(transition_token, save_directive.transition_token());
  EXPECT_FALSE(save_directive.maybe_cross_frame_sink());

  auto animate_directive = CompositorFrameTransitionDirective::CreateAnimate(
      transition_token, /*maybe_cross_frame_sink=*/true, 2);

  EXPECT_EQ(2u, animate_directive.sequence_id());
  EXPECT_EQ(Type::kAnimateRenderer, animate_directive.type());
  EXPECT_EQ(transition_token, animate_directive.transition_token());
  EXPECT_TRUE(animate_directive.maybe_cross_frame_sink());
}

}  // namespace
}  // namespace viz
