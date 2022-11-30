// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_display_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(CastDisplayScaleTest, StandardResolutions_16x9) {
  const gfx::Size kResolution720p(1280, 720);
  const gfx::Size kResolution1080p(1920, 1080);
  const gfx::Size kResolution1440p(2560, 1440);
  const gfx::Size kResolution2160p(3840, 2160);
  const gfx::Size kResolution4320p(7680, 4320);

  EXPECT_EQ(1.f, GetDeviceScaleFactor(kResolution720p));
  EXPECT_EQ(1.5f, GetDeviceScaleFactor(kResolution1080p));
  EXPECT_EQ(2.f, GetDeviceScaleFactor(kResolution1440p));
  EXPECT_EQ(3.f, GetDeviceScaleFactor(kResolution2160p));
  EXPECT_EQ(6.f, GetDeviceScaleFactor(kResolution4320p));
}

TEST(CastDisplayScaleTest, NonstandardResolutions) {
  const gfx::Size kResolutionHomeHub(600, 1024);
  EXPECT_EQ(1.f, GetDeviceScaleFactor(kResolutionHomeHub));
}

}  // namespace chromecast
