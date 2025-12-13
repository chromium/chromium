// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"

#include <ImageIO/ImageIO.h>

#include <numeric>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {

TEST(IconUtilsTests, MaskSquareSolidBackgroundDiyAppIcon) {
  // This test simulates a square icon without rounded corners
  // and a solid color square background. It will enter the mask logic
  // for maskable icons, determining if the output from MaskDiyAppIcon()
  // matches the pre-generated target icon.
  gfx::Image square_icon = web_app::test::LoadTestImageFromDisk(
      base::FilePath("chrome/test/data/web_apps/blue-192.png"));
  ASSERT_TRUE(!square_icon.IsEmpty());
  gfx::Image expected_masked_icon = web_app::test::LoadTestImageFromDisk(
      base::FilePath("chrome/test/data/web_apps/masked_blue-192.png"));
  ASSERT_TRUE(!expected_masked_icon.IsEmpty());
  gfx::Image actual_masked_icon = MaskDiyAppIcon(square_icon);
  ASSERT_TRUE(!actual_masked_icon.IsEmpty());
  ASSERT_TRUE(gfx::test::AreImagesClose(actual_masked_icon,
                                        expected_masked_icon,
                                        /*max_deviation=*/20));
}

TEST(IconUtilsTests, MaskNormalDiyAppIcon) {
  // Test that a DIY icon without a solid background or rounded background is
  // masked after zooming out.

  // Small favicon
  gfx::Image original_favicon = web_app::test::LoadTestImageFromDisk(
      base::FilePath("chrome/test/data/web_apps/pattern3-256.png"));
  ASSERT_TRUE(!original_favicon.IsEmpty());
  gfx::Image expected_masked_favicon = web_app::test::LoadTestImageFromDisk(
      base::FilePath("chrome/test/data/web_apps/masked_pattern3-256.png"));
  ASSERT_TRUE(!expected_masked_favicon.IsEmpty());
  gfx::Image actual_masked_favicon = MaskDiyAppIcon(original_favicon);
  ASSERT_TRUE(!actual_masked_favicon.IsEmpty());
  ASSERT_TRUE(gfx::test::AreImagesClose(actual_masked_favicon,
                                        expected_masked_favicon,
                                        /*max_deviation=*/5));
}
}  // namespace web_app
