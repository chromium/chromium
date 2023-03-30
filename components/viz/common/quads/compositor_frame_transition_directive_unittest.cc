// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using Type = CompositorFrameTransitionDirective::Type;

TEST(CompositorFrameTransitionDirective, GettersReflectParameters) {
  auto navigation_id = NavigationID::Create();
  auto save_directive =
      CompositorFrameTransitionDirective::CreateSave(navigation_id, 1u, {});

  EXPECT_EQ(1u, save_directive.sequence_id());
  EXPECT_EQ(Type::kSave, save_directive.type());
  EXPECT_EQ(navigation_id, save_directive.navigation_id());

  auto animate_directive =
      CompositorFrameTransitionDirective::CreateAnimate(navigation_id, 2);

  EXPECT_EQ(2u, animate_directive.sequence_id());
  EXPECT_EQ(Type::kAnimateRenderer, animate_directive.type());
  EXPECT_EQ(navigation_id, animate_directive.navigation_id());
}

}  // namespace
}  // namespace viz
