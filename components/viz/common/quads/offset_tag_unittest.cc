// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/offset_tag.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {

TEST(OffsetTagConstraints, Clamp) {
  OffsetTagConstraints constraints(-10.0f, 20.0f, -30.0f, 40.0f);

  // Verify that min and max offset are not clamped.
  gfx::Vector2dF min_offset(-10.0, -30.0f);
  gfx::Vector2dF max_offset(20.0, 40.0f);
  EXPECT_EQ(min_offset, constraints.Clamp(min_offset));
  EXPECT_EQ(max_offset, constraints.Clamp(max_offset));

  // Verify that an offset smaller than min or bigger than max are clamped.
  EXPECT_EQ(constraints.Clamp(gfx::Vector2dF(-11.0, -30.0f)), min_offset);
  EXPECT_EQ(constraints.Clamp(gfx::Vector2dF(-10.0, -31.0f)), min_offset);
  EXPECT_EQ(constraints.Clamp(gfx::Vector2dF(-1000.0, -1000.0f)), min_offset);
  EXPECT_EQ(constraints.Clamp(gfx::Vector2dF(21.0f, 40.0f)), max_offset);
  EXPECT_EQ(constraints.Clamp(gfx::Vector2dF(21.0f, 40.0f)), max_offset);
  EXPECT_EQ(constraints.Clamp(gfx::Vector2dF(1000.0f, 1000.0f)), max_offset);
}

TEST(OffsetTagConstraints, ExpandVisibleRect) {
  {
    OffsetTagConstraints constraints(-10.0f, 20.0f, -30.0f, 40.0f);
    gfx::RectF rect(100.0, 100.0f);
    constraints.ExpandVisibleRect(rect);
    EXPECT_EQ(rect, gfx::RectF(-20.0f, -40.0f, 130.0f, 170.0f));
  }

  {
    OffsetTagConstraints constraints(0, 0, -147, 0);
    gfx::RectF rect(1080.0, 2210.0f);
    constraints.ExpandVisibleRect(rect);
    EXPECT_EQ(rect, gfx::RectF(0, 0, 1080.0f, 2357.0f));
  }
}

}  // namespace viz
