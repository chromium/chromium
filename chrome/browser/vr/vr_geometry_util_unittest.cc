// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_geometry_util.h"

#include "chrome/browser/vr/test/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

TEST(VrGeometryUtilTest, CalculateScreenSize) {
  gfx::SizeF size(2.4f, 1.6f);

  gfx::SizeF screen_size =
      CalculateScreenSize(GetPixelDaydreamProjMatrix(), 2.5f, size);

  EXPECT_FLOAT_EQ(screen_size.width(), 0.49592164f);
  EXPECT_FLOAT_EQ(screen_size.height(), 0.27598655f);
}

}  // namespace vr
