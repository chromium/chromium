// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_background_ash_impl.h"

#include "base/i18n/rtl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace media_message_center {

class MediaNotificationBackgroundAshImplTest : public testing::Test {
 public:
  MediaNotificationBackgroundAshImplTest() = default;
  ~MediaNotificationBackgroundAshImplTest() override = default;

  void SetUp() override {
    background_ = std::make_unique<MediaNotificationBackgroundAshImpl>();
  }

  void TearDown() override { background_.reset(); }

  gfx::Rect GetArtworkBounds(gfx::Rect view_rect) {
    return background_->GetArtworkBounds(view_rect);
  }

  MediaNotificationBackgroundAshImpl* background() { return background_.get(); }

 private:
  std::unique_ptr<MediaNotificationBackgroundAshImpl> background_;
};

TEST_F(MediaNotificationBackgroundAshImplTest, ArtworkBoundsTest) {
  gfx::Rect parent_bounds(0, 0, 100, 100);
  background()->UpdateArtwork(gfx::test::CreateImageSkia(120, 60));
  EXPECT_EQ(GetArtworkBounds(parent_bounds), gfx::Rect(-36, 4, 160, 80));

  background()->UpdateArtwork(gfx::test::CreateImageSkia(40, 50));
  EXPECT_EQ(GetArtworkBounds(parent_bounds), gfx::Rect(4, -6, 80, 100));

  background()->UpdateArtwork(gfx::test::CreateImageSkia(80, 120));
  EXPECT_EQ(GetArtworkBounds(parent_bounds), gfx::Rect(4, -16, 80, 120));

  base::i18n::SetRTLForTesting(true);
  background()->UpdateArtwork(gfx::test::CreateImageSkia(80, 40));
  EXPECT_EQ(GetArtworkBounds(parent_bounds), gfx::Rect(-24, 4, 160, 80));
}

}  // namespace media_message_center
