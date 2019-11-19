// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon_button.h"

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(VectorIconButton, CornerRadiiOnResize) {
  VectorIconButton button(base::RepeatingCallback<void()>(),
                          vector_icons::kMicIcon, nullptr);
  button.SetSize(1.0f, 1.0f);
  button.SetCornerRadius(0.25f);

  EXPECT_FLOAT_EQ(0.25f, button.corner_radius());
  // The foreground of vector icons is not not automatically affected by corner
  // radius.
  EXPECT_FLOAT_EQ(0.0f, button.foreground()->corner_radius());
  EXPECT_FLOAT_EQ(0.25f, button.background()->corner_radius());
  EXPECT_FLOAT_EQ(0.25f, button.hit_plane()->corner_radius());

  button.SetSize(2.0f, 2.0f);

  EXPECT_FLOAT_EQ(0.25f, button.corner_radius());
  EXPECT_FLOAT_EQ(0.0f, button.foreground()->corner_radius());
  EXPECT_FLOAT_EQ(0.25f, button.background()->corner_radius());
  EXPECT_FLOAT_EQ(0.25f, button.hit_plane()->corner_radius());
}

}  // namespace vr
