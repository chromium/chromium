// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/system_media_controls/mac/now_playing_info_center_delegate_cocoa.h"

#import <MediaPlayer/MediaPlayer.h>

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace system_media_controls::internal {

class NowPlayingInfoCenterDelegateCocoaTest : public testing::Test {
 protected:
  void SetUp() override {
    delegate_ = [[NowPlayingInfoCenterDelegateCocoa alloc] init];
  }

  void TearDown() override { [delegate_ resetNowPlayingInfo]; }

  NowPlayingInfoCenterDelegateCocoa* __strong delegate_;
};

TEST_F(NowPlayingInfoCenterDelegateCocoaTest,
       ClearPositionPreservesPublicationState) {
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  [delegate_ setTitle:@"title"];
  [delegate_ setElapsedPlaybackTime:@5];
  [delegate_ updateNowPlayingInfo];

  [delegate_ clearPosition];

  ASSERT_NE(nil, center.nowPlayingInfo);
  EXPECT_NSEQ(@"title", center.nowPlayingInfo[MPMediaItemPropertyTitle]);
  EXPECT_EQ(nil,
            center.nowPlayingInfo[MPNowPlayingInfoPropertyElapsedPlaybackTime]);

  [delegate_ clearMetadata];
  ASSERT_EQ(nil, center.nowPlayingInfo);

  [delegate_ clearPosition];
  EXPECT_EQ(nil, center.nowPlayingInfo);
}

}  // namespace system_media_controls::internal
