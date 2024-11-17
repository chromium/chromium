// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"

#include <ImageIO/ImageIO.h>

#include <numeric>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {

namespace {

gfx::Image LoadTestPNG(const base::FilePath::CharType* path) {
  base::FilePath data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_root);
  base::FilePath image_path = data_root.Append(path);
  std::string png_data;
  ReadFileToString(image_path, &png_data);
  return gfx::Image::CreateFrom1xPNGBytes(base::as_byte_span(png_data));
}

}  // namespace

TEST(IconUtilsTests, MaskSquareSolidBackgroundDiyAppIcon) {
  // This test simulates a square ChatGPT icon without rounded corners
  // and a solid color square background. It will enter the mask logic
  // for maskable icons, determining if the output from MaskDiyAppIcon()
  // matches the pre-generated target icon.
  gfx::Image chatgpt_icon = LoadTestPNG(
      FILE_PATH_LITERAL("chrome/test/data/web_apps/chatgpt_icon.png"));
  ASSERT_TRUE(!chatgpt_icon.IsEmpty());
  gfx::Image masked_chatgpt_icon = LoadTestPNG(
      FILE_PATH_LITERAL("chrome/test/data/web_apps/masked_chatgpt_icon.png"));
  // ASSERT_TRUE(!masked_chatgpt_icon.IsEmpty());
  gfx::Image masked_algorithm_chatgpt_icon = MaskDiyAppIcon(chatgpt_icon);
  ASSERT_TRUE(!masked_algorithm_chatgpt_icon.IsEmpty());
  ASSERT_TRUE(gfx::test::AreImagesEqual(masked_algorithm_chatgpt_icon,
                                        masked_chatgpt_icon));
}

TEST(IconUtilsTests, MaskNormalDiyAppIcon) {
  // Test that a DIY icon without a solid background or rounded background is
  // masked after zooming out.

  // Small favicon
  gfx::Image rounded_slack_16x16_icon = LoadTestPNG(
      FILE_PATH_LITERAL("chrome/test/data/web_apps/slack_16x16_favicon.png"));
  ASSERT_TRUE(!rounded_slack_16x16_icon.IsEmpty());
  gfx::Image masked_slack_16x16_favicon = LoadTestPNG(FILE_PATH_LITERAL(
      "chrome/test/data/web_apps/masked_slack_16x16_favicon.png"));
  ASSERT_TRUE(!masked_slack_16x16_favicon.IsEmpty());
  gfx::Image masked_algorithm_slack_16x16_icon =
      MaskDiyAppIcon(rounded_slack_16x16_icon);
  ASSERT_TRUE(!masked_algorithm_slack_16x16_icon.IsEmpty());
  ASSERT_TRUE(gfx::test::AreImagesEqual(masked_algorithm_slack_16x16_icon,
                                        masked_slack_16x16_favicon));

  // Real favicon
  gfx::Image rounded_slack_icon = LoadTestPNG(
      FILE_PATH_LITERAL("chrome/test/data/web_apps/slack_favicon.png"));
  ASSERT_TRUE(!rounded_slack_icon.IsEmpty());
  gfx::Image masked_slack_icon = LoadTestPNG(
      FILE_PATH_LITERAL("chrome/test/data/web_apps/masked_slack_favicon.png"));
  ASSERT_TRUE(!masked_slack_icon.IsEmpty());
  gfx::Image masked_algorithm_slack_icon = MaskDiyAppIcon(rounded_slack_icon);
  ASSERT_TRUE(!masked_algorithm_slack_icon.IsEmpty());
  ASSERT_TRUE(gfx::test::AreImagesEqual(masked_algorithm_slack_icon,
                                        masked_slack_icon));
}

}  // namespace web_app
