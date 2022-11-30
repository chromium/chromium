// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/button.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(Button, Hover) {
  base::RepeatingCallback<void()> callback;
  Button button(callback, nullptr);
  button.set_hover_offset(0.0f);
  button.SetSize(1.0f, 1.0f);

  gfx::Transform xform = button.hit_plane()->LocalTransform();

  button.OnHoverEnter(gfx::PointF(0.5f, 0.5f), base::TimeTicks());
  EXPECT_EQ(xform.ToString(), button.hit_plane()->LocalTransform().ToString());
  button.OnHoverLeave(base::TimeTicks());

  button.set_hover_offset(0.04f);
  button.OnHoverEnter(gfx::PointF(0.5f, 0.5f), base::TimeTicks());
  EXPECT_NE(xform.ToString(), button.hit_plane()->LocalTransform().ToString());
  button.OnHoverLeave(base::TimeTicks());

  button.SetEnabled(false);
  button.OnHoverEnter(gfx::PointF(0.5f, 0.5f), base::TimeTicks());
  EXPECT_EQ(xform.ToString(), button.hit_plane()->LocalTransform().ToString());
  button.OnHoverLeave(base::TimeTicks());
}

}  // namespace vr
