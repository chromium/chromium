// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_background_ash_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media_message_center {

class MediaNotificationBackgroundAshImplTest : public testing::Test {
 public:
  MediaNotificationBackgroundAshImplTest() = default;
  ~MediaNotificationBackgroundAshImplTest() override = default;

  void SetUp() override {
    background_ = std::make_unique<MediaNotificationBackgroundAshImpl>();
  }

  void TearDown() override { background_.reset(); }

  gfx::ImageSkia CreateTestImage(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  gfx::Rect GetArtworkBounds(gfx::Rect view_rect) {
    return background_->GetArtworkBounds(view_rect);
  }

  MediaNotificationBackgroundAshImpl* background() { return background_.get(); }

 private:
  std::unique_ptr<MediaNotificationBackgroundAshImpl> background_;
};

TEST_F(MediaNotificationBackgroundAshImplTest, ArtworkBoundsTest) {
  gfx::Rect parent_bounds(0, 0, 100, 100);
  background()->UpdateArtwork(CreateTestImage(160, 60));
  EXPECT_EQ(GetArtworkBounds(parent_bounds).size(), gfx::Size(80, 30));

  background()->UpdateArtwork(CreateTestImage(60, 160));
  EXPECT_EQ(GetArtworkBounds(parent_bounds).size(), gfx::Size(30, 80));

  background()->UpdateArtwork(CreateTestImage(40, 20));
  EXPECT_EQ(GetArtworkBounds(parent_bounds).size(), gfx::Size(80, 40));
}

}  // namespace media_message_center
