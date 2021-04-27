// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace qrcode_generator {

class QRCodeGeneratorBubbleControllerTest : public testing::Test {
 public:
  QRCodeGeneratorBubbleControllerTest() = default;

  ~QRCodeGeneratorBubbleControllerTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QRCodeGeneratorBubbleControllerTest);
};

TEST_F(QRCodeGeneratorBubbleControllerTest, AllowedURLs) {
  scoped_feature_list_.InitAndEnableFeature(kSharingQRCodeGenerator);

  // Allow valid http/https URLs.
  ASSERT_TRUE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("http://www.example.com")));
  ASSERT_TRUE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("https://www.example.com")));
  ASSERT_TRUE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("https://www.example.com/path?q=abc")));

  // Disallow browser-ui URLs.
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("about:blank")));
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("chrome://newtab")));
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("chrome://settings")));

  // Disallow invalid URLs.
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(GURL("")));
  ASSERT_FALSE(
      QRCodeGeneratorBubbleController::IsGeneratorAvailable(GURL("NotAURL")));
}

TEST_F(QRCodeGeneratorBubbleControllerTest, UnavailableWithFeatureOff) {
  scoped_feature_list_.InitAndDisableFeature(kSharingQRCodeGenerator);

  // Normally-available URLs should not be allowed when the feature is off.
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("http://www.example.com")));
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("https://www.example.com")));
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(
      GURL("https://www.example.com/path?q=abc")));
}

}  // namespace qrcode_generator
