// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using Type = CompositorFrameTransitionDirective::Type;

TEST(CompositorFrameTransitionDirective, GettersReflectParameters) {
  CompositorFrameTransitionDirective save_directive(1u, Type::kSave);

  EXPECT_EQ(1u, save_directive.sequence_id());
  EXPECT_EQ(Type::kSave, save_directive.type());

  CompositorFrameTransitionDirective animate_directive(2,
                                                       Type::kAnimateRenderer);

  EXPECT_EQ(2u, animate_directive.sequence_id());
  EXPECT_EQ(Type::kAnimateRenderer, animate_directive.type());
}

}  // namespace
}  // namespace viz
