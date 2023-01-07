// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_util.h"

#include <wayland-server-protocol-core.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

namespace exo {
namespace wayland {
namespace {

TEST(WaylandDisplayUtilTest, OutputTransformSimple) {
  EXPECT_EQ(OutputTransform(display::Display::ROTATE_0),
            WL_OUTPUT_TRANSFORM_NORMAL);
  EXPECT_EQ(OutputTransform(display::Display::ROTATE_90),
            WL_OUTPUT_TRANSFORM_270);
  EXPECT_EQ(OutputTransform(display::Display::ROTATE_180),
            WL_OUTPUT_TRANSFORM_180);
  EXPECT_EQ(OutputTransform(display::Display::ROTATE_270),
            WL_OUTPUT_TRANSFORM_90);
}

}  // namespace
}  // namespace wayland
}  // namespace exo
