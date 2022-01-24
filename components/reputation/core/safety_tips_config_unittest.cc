// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/reputation/core/safety_tips_config.h"

#include "components/reputation/core/safety_tip_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace reputation {

TEST(SafetyTipsConfigTest, TestUrlAllowlist) {
  SetSafetyTipAllowlistPatterns({"example.com/"}, {}, {});
  auto* config = GetSafetyTipsRemoteConfigProto();
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.com")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.org")));
}

TEST(SafetyTipsConfigTest, TestTargetUrlAllowlist) {
  SetSafetyTipAllowlistPatterns({}, {"exa.*\\.com"}, {});
  auto* config = GetSafetyTipsRemoteConfigProto();
  EXPECT_TRUE(
      IsTargetHostAllowlistedBySafetyTipsComponent(config, "example.com"));
  EXPECT_FALSE(
      IsTargetHostAllowlistedBySafetyTipsComponent(config, "example.org"));
}

TEST(SafetyTipsConfigTest, TestCommonWords) {
  // IsCommonWordInConfigProto does a binary search of sorted common words.
  SetSafetyTipAllowlistPatterns({}, {}, {"common3", "common1", "common2"});
  auto* config = GetSafetyTipsRemoteConfigProto();
  EXPECT_TRUE(IsCommonWordInConfigProto(config, "common1"));
  EXPECT_TRUE(IsCommonWordInConfigProto(config, "common2"));
  EXPECT_TRUE(IsCommonWordInConfigProto(config, "common3"));
  EXPECT_FALSE(IsCommonWordInConfigProto(config, "uncommon"));
}

}  // namespace reputation
