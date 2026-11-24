// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/android/omnibox_icon_bridge.h"

#include "components/omnibox/browser/omnibox_icon_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {

namespace {

using ::testing::_;
using ::testing::Return;

constexpr IconResourceIds kIconId = IconResourceIds::GLOBE;
// Not in this client's IconResourceIds, like an id newer than the client.
constexpr IconResourceIds kUnknownToClientId =
    static_cast<IconResourceIds>(16000);

class MockOmniboxIconSource : public OmniboxIconSource {
 public:
  MOCK_METHOD(std::optional<SkBitmap>,
              RenderBitmap,
              (IconResourceIds id, int size_px, SkColor tint_color),
              (override));
  MOCK_METHOD(bool, IsMultiColor, (IconResourceIds id), (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

SkBitmap CreateBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(4, 4);
  return bitmap;
}

}  // namespace

class OmniboxIconBridgeTest : public testing::Test {
 protected:
  testing::NiceMock<MockOmniboxIconSource> source_;
  base::AutoReset<OmniboxIconSource*> reset_ =
      SetOmniboxIconSourceForTesting(&source_);
};

TEST_F(OmniboxIconBridgeTest, RendersIconWithColorMode) {
  EXPECT_CALL(source_, RenderBitmap(kIconId, 63, SK_ColorBLACK))
      .WillOnce(Return(CreateBitmap()));
  EXPECT_CALL(source_, IsMultiColor(kIconId)).WillOnce(Return(true));

  std::optional<RenderedOmniboxIcon> icon =
      RenderOmniboxIconForAndroid(kIconId, /*size_px=*/63);

  ASSERT_TRUE(icon);
  EXPECT_FALSE(icon->bitmap.drawsNothing());
  EXPECT_TRUE(icon->is_multi_color);
}

TEST_F(OmniboxIconBridgeTest, ReturnsNothingWhenSourceHasNoIcon) {
  EXPECT_CALL(source_, RenderBitmap(kIconId, 24, SK_ColorBLACK))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(source_, IsMultiColor).Times(0);
  EXPECT_FALSE(RenderOmniboxIconForAndroid(kIconId, /*size_px=*/24));
}

TEST_F(OmniboxIconBridgeTest, EmptyBitmapReturnsNothing) {
  EXPECT_CALL(source_, RenderBitmap).WillOnce(Return(SkBitmap()));
  EXPECT_CALL(source_, IsMultiColor).Times(0);
  EXPECT_FALSE(RenderOmniboxIconForAndroid(kIconId, /*size_px=*/24));
}

TEST_F(OmniboxIconBridgeTest, IdsUnknownToClientReachSource) {
  ASSERT_FALSE(IconResourceIds_IsValid(kUnknownToClientId));
  EXPECT_CALL(source_, RenderBitmap(kUnknownToClientId, 24, _))
      .WillOnce(Return(CreateBitmap()));
  EXPECT_TRUE(RenderOmniboxIconForAndroid(kUnknownToClientId, /*size_px=*/24));
}

TEST_F(OmniboxIconBridgeTest, InvalidSizeSkipsSource) {
  EXPECT_CALL(source_, RenderBitmap).Times(0);
  EXPECT_FALSE(RenderOmniboxIconForAndroid(kIconId, /*size_px=*/0));
}

}  // namespace omnibox
