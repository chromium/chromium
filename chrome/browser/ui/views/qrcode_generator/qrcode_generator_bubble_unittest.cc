// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace qrcode_generator {

namespace {

using QRCodeGeneratorBubbleTest = testing::Test;

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

TEST_F(QRCodeGeneratorBubbleTest, GeneratedCodeHasQuietZone) {
  const int kBaseSizeDip = 16;
  const int kQuietZoneTiles = 4;
  const int kTileToDip = 2;
  const int kQuietZoneDip = kQuietZoneTiles * kTileToDip;

  SkBitmap base_bitmap;
  base_bitmap.allocN32Pixels(kBaseSizeDip, kBaseSizeDip);
  base_bitmap.eraseColor(SK_ColorRED);
  auto base_image = gfx::ImageSkia::CreateFrom1xBitmap(base_bitmap);

  auto image = QRCodeGeneratorBubble::AddQRCodeQuietZone(
      base_image,
      gfx::Size(kBaseSizeDip / kTileToDip, kBaseSizeDip / kTileToDip));

  EXPECT_EQ(base_image.width(), kBaseSizeDip);
  EXPECT_EQ(base_image.height(), kBaseSizeDip);
  EXPECT_EQ(image.width(), kBaseSizeDip + kQuietZoneDip * 2);
  EXPECT_EQ(image.height(), kBaseSizeDip + kQuietZoneDip * 2);

  EXPECT_EQ(SK_ColorRED, base_image.bitmap()->getColor(0, 0));

  EXPECT_EQ(SK_ColorWHITE, image.bitmap()->getColor(0, 0));
  EXPECT_EQ(SK_ColorWHITE,
            image.bitmap()->getColor(kQuietZoneDip, kQuietZoneDip - 1));
  EXPECT_EQ(SK_ColorWHITE,
            image.bitmap()->getColor(kQuietZoneDip - 1, kQuietZoneDip));
  EXPECT_EQ(SK_ColorRED,
            image.bitmap()->getColor(kQuietZoneDip, kQuietZoneDip));
}

}  // namespace

}  // namespace qrcode_generator
