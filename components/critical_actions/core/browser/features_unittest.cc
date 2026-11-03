// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions::features {
namespace {

TEST(CriticalActionsFeaturesTest, FeatureDisabledDisablesAllChatLinkouts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCriticalActionHistory);

  EXPECT_FALSE(IsChatLinkoutsEnabled());
  EXPECT_FALSE(IsActionLevelChatLinkoutsEnabled());
}

TEST(CriticalActionsFeaturesTest, DefaultParamsWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCriticalActionHistory);

  EXPECT_TRUE(kEnableChatLinkouts.Get());
  EXPECT_FALSE(kEnableActionLevelChatLinkouts.Get());
  EXPECT_TRUE(IsChatLinkoutsEnabled());
  EXPECT_FALSE(IsActionLevelChatLinkoutsEnabled());
  EXPECT_EQ(kMaxTaskToAnchorCacheCapacity.Get(), 200);
}

TEST(CriticalActionsFeaturesTest,
     ActionLevelChatLinkoutsEnabledWhenBothParamsTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCriticalActionHistory, {{"enable_chat_linkouts", "true"},
                               {"enable_action_level_chat_linkouts", "true"}});

  EXPECT_TRUE(IsChatLinkoutsEnabled());
  EXPECT_TRUE(IsActionLevelChatLinkoutsEnabled());
}

TEST(CriticalActionsFeaturesTest,
     ActionLevelChatLinkoutsIneffectiveWhenChatLinkoutsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCriticalActionHistory, {{"enable_chat_linkouts", "false"},
                               {"enable_action_level_chat_linkouts", "true"}});

  EXPECT_FALSE(IsChatLinkoutsEnabled());
  EXPECT_FALSE(IsActionLevelChatLinkoutsEnabled());
}

TEST(CriticalActionsFeaturesTest, MaxTaskToAnchorCacheCapacityOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kCriticalActionHistory, {{"max_task_to_anchor_cache_capacity", "500"}});

  EXPECT_EQ(kMaxTaskToAnchorCacheCapacity.Get(), 500);
}

}  // namespace
}  // namespace critical_actions::features
