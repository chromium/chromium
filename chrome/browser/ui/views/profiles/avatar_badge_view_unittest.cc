// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_badge_view.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/profiles/profile_view_avatar_decoration_specs.h"
#include "testing/gtest/include/gtest/gtest.h"

using AvatarBadgeViewTest = testing::Test;

TEST_F(AvatarBadgeViewTest, DefaultTierWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnabledAiSubscriptionTierNameOverride);

  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(1),
            std::u16string(kAvatarBadgeLabelTier1));
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(2),
            std::u16string(kAvatarBadgeLabelTier2));
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(3),
            std::u16string(kAvatarBadgeLabelTier3));
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(0), std::u16string());
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(99), std::u16string());
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(-1), std::u16string());
}

TEST_F(AvatarBadgeViewTest, FullTierOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping",
        R"({"1": "Custom Tier 1", "2": "Custom Tier 2", "3": "Custom Tier 3"})"}});

  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(1), u"Custom Tier 1");
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(2), u"Custom Tier 2");
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(3), u"Custom Tier 3");
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(99), std::u16string());
}

TEST_F(AvatarBadgeViewTest, PartialTierOverrideWithFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", R"({"2": "Advanced"})"}});

  // Tier 1 and 3 should fall back to codebase defaults.
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(1),
            std::u16string(kAvatarBadgeLabelTier1));
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(2), u"Advanced");
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(3),
            std::u16string(kAvatarBadgeLabelTier3));
}

TEST_F(AvatarBadgeViewTest, ExplicitEmptyString) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", R"({"1": "", "2": "Advanced"})"}});

  // Explicit empty string in mapping should return empty string.
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(1), std::u16string());
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(2), u"Advanced");
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(3),
            std::u16string(kAvatarBadgeLabelTier3));
}

TEST_F(AvatarBadgeViewTest, InvalidJsonFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", "{not-valid-json"}});

  // Malformed JSON should safely fall back to codebase defaults.
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(1),
            std::u16string(kAvatarBadgeLabelTier1));
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(2),
            std::u16string(kAvatarBadgeLabelTier2));
  EXPECT_EQ(AvatarBadgeView::GetAvatarBadgeLabel(3),
            std::u16string(kAvatarBadgeLabelTier3));
}
