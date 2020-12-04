// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using Effect = CompositorFrameTransitionDirective::Effect;
using Type = CompositorFrameTransitionDirective::Type;

TEST(CompositorFrameTransitionDirective, GettersReflectParameters) {
  CompositorFrameTransitionDirective save_directive(
      1u, Type::kSave, Effect::kCoverLeft,
      base::TimeDelta::FromMilliseconds(100));

  EXPECT_EQ(1u, save_directive.sequence_id());
  EXPECT_EQ(Type::kSave, save_directive.type());
  EXPECT_EQ(Effect::kCoverLeft, save_directive.effect());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(100), save_directive.duration());

  CompositorFrameTransitionDirective animate_directive(2, Type::kAnimate);

  EXPECT_EQ(2u, animate_directive.sequence_id());
  EXPECT_EQ(Type::kAnimate, animate_directive.type());
}

}  // namespace
}  // namespace viz
