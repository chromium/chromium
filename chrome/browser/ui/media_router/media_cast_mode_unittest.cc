// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_cast_mode.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Not;
using testing::HasSubstr;

namespace media_router {

TEST(MediaCastModeTest, MediaCastModeToDescription) {
  EXPECT_FALSE(
      MediaCastModeToDescription(MediaCastMode::PRESENTATION, "youtube.com")
          .empty());
  EXPECT_FALSE(
      MediaCastModeToDescription(MediaCastMode::TAB_MIRROR, "").empty());
  EXPECT_FALSE(
      MediaCastModeToDescription(MediaCastMode::DESKTOP_MIRROR, "").empty());
}

TEST(MediaCastModeTest, IsValidCastModeNum) {
  EXPECT_TRUE(IsValidCastModeNum(MediaCastMode::PRESENTATION));
  EXPECT_TRUE(IsValidCastModeNum(MediaCastMode::TAB_MIRROR));
  EXPECT_TRUE(IsValidCastModeNum(MediaCastMode::DESKTOP_MIRROR));
  EXPECT_FALSE(IsValidCastModeNum(-1));
  EXPECT_FALSE(IsValidCastModeNum(666));
}

}  // namespace media_router
