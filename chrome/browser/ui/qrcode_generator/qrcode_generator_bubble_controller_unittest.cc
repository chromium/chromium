// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"

#include "base/test/scoped_feature_list.h"
#include "components/sharing_message/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace qrcode_generator {

class QRCodeGeneratorBubbleControllerTest : public testing::Test {
 public:
  QRCodeGeneratorBubbleControllerTest() = default;

  QRCodeGeneratorBubbleControllerTest(
      const QRCodeGeneratorBubbleControllerTest&) = delete;
  QRCodeGeneratorBubbleControllerTest& operator=(
      const QRCodeGeneratorBubbleControllerTest&) = delete;

  ~QRCodeGeneratorBubbleControllerTest() override = default;
};

TEST_F(QRCodeGeneratorBubbleControllerTest, AllowedURLs) {
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

}  // namespace qrcode_generator
