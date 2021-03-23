// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace qrcode_generator {

namespace {

class QRCodeGeneratorBubbleTest : public testing::Test {
 public:
  QRCodeGeneratorBubbleTest() = default;
  ~QRCodeGeneratorBubbleTest() override = default;
};

TEST_F(QRCodeGeneratorBubbleTest, SuggestedDownloadURLNoIP) {
  EXPECT_EQ(QRCodeGeneratorBubble::GetQRCodeFilenameForURL(GURL("10.1.2.3")),
            u"qrcode_chrome.png");

  EXPECT_EQ(QRCodeGeneratorBubble::GetQRCodeFilenameForURL(
                GURL("https://chromium.org")),
            u"qrcode_chromium.org.png");

  EXPECT_EQ(
      QRCodeGeneratorBubble::GetQRCodeFilenameForURL(GURL("text, not url")),
      u"qrcode_chrome.png");
}

}  // namespace

}  // namespace qrcode_generator
