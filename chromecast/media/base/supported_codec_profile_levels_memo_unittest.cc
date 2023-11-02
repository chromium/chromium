// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/supported_codec_profile_levels_memo.h"

#include "chromecast/public/media/decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class SupportedCodecProfileLevelsMemoTest : public ::testing::Test {
 protected:
  SupportedCodecProfileLevelsMemo memo_;
};

TEST_F(SupportedCodecProfileLevelsMemoTest,
       IsSupportedVideoConfig_NoProfilesAdded) {
  EXPECT_FALSE(memo_.IsSupportedVideoConfig(kCodecH264, kH264Baseline, 10));
  EXPECT_FALSE(memo_.IsSupportedVideoConfig(kCodecHEVC, kHEVCMain, 30));
  EXPECT_FALSE(memo_.IsSupportedVideoConfig(kCodecVP8, kVP8ProfileAny, 0));
  EXPECT_FALSE(memo_.IsSupportedVideoConfig(kCodecVP9, kVP9Profile0, 10));
}

TEST_F(SupportedCodecProfileLevelsMemoTest,
       IsSupportedVideoConfig_ProfileAdded) {
  memo_.AddSupportedCodecProfileLevel({kCodecH264, kH264Baseline, 10});
  EXPECT_TRUE(memo_.IsSupportedVideoConfig(kCodecH264, kH264Baseline, 10));
  EXPECT_FALSE(memo_.IsSupportedVideoConfig(kCodecH264, kH264Main, 10));
}

}  // namespace media
}  // namespace chromecast
