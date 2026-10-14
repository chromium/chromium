// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/ai_subscription_tier_names.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subscription_eligibility {
namespace {

// Returns the built-in short name of `tier`. The built-in names differ between
// Google Chrome and Chromium builds, so the tests compare against them instead
// of hardcoding the names.
std::u16string DefaultShortName(int tier) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnabledAiSubscriptionTierNameOverride);
  return GetAiSubscriptionTierShortName(tier);
}

// Returns the built-in full name of `tier`. See `DefaultShortName()`.
std::u16string DefaultFullName(int tier) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnabledAiSubscriptionTierNameOverride);
  return GetAiSubscriptionTierFullName(tier);
}

using AiSubscriptionTierNamesTest = testing::Test;

TEST_F(AiSubscriptionTierNamesTest, DefaultShortNamesWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnabledAiSubscriptionTierNameOverride);

  const std::u16string tier1 = GetAiSubscriptionTierShortName(1);
  const std::u16string tier2 = GetAiSubscriptionTierShortName(2);
  const std::u16string tier3 = GetAiSubscriptionTierShortName(3);
  EXPECT_FALSE(tier1.empty());
  EXPECT_FALSE(tier2.empty());
  EXPECT_FALSE(tier3.empty());
  EXPECT_NE(tier1, tier2);
  EXPECT_NE(tier1, tier3);
  EXPECT_NE(tier2, tier3);
}

TEST_F(AiSubscriptionTierNamesTest, FullShortNameOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping",
        R"({"1": "Custom 1", "2": "Custom 2", "3": "Custom 3"})"}});

  EXPECT_EQ(GetAiSubscriptionTierShortName(1), u"Custom 1");
  EXPECT_EQ(GetAiSubscriptionTierShortName(2), u"Custom 2");
  EXPECT_EQ(GetAiSubscriptionTierShortName(3), u"Custom 3");
  EXPECT_EQ(GetAiSubscriptionTierShortName(99), std::u16string());
}

TEST_F(AiSubscriptionTierNamesTest, PartialShortNameOverrideWithFallback) {
  const std::u16string default1 = DefaultShortName(1);
  const std::u16string default3 = DefaultShortName(3);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", R"({"2": "Advanced"})"}});

  // Tier 1 and 3 should fall back to codebase defaults.
  EXPECT_EQ(GetAiSubscriptionTierShortName(1), default1);
  EXPECT_EQ(GetAiSubscriptionTierShortName(2), u"Advanced");
  EXPECT_EQ(GetAiSubscriptionTierShortName(3), default3);
}

TEST_F(AiSubscriptionTierNamesTest, OverrideCanNameUnknownTier) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", R"({"4": "Max"})"}});

  EXPECT_EQ(GetAiSubscriptionTierShortName(4), u"Max");
}

TEST_F(AiSubscriptionTierNamesTest, ExplicitEmptyString) {
  const std::u16string default3 = DefaultShortName(3);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", R"({"1": "", "2": "Advanced"})"}});

  // Explicit empty string in mapping should return empty string.
  EXPECT_EQ(GetAiSubscriptionTierShortName(1), std::u16string());
  EXPECT_EQ(GetAiSubscriptionTierShortName(2), u"Advanced");
  EXPECT_EQ(GetAiSubscriptionTierShortName(3), default3);
}

TEST_F(AiSubscriptionTierNamesTest, InvalidJsonFallback) {
  const std::u16string default1 = DefaultShortName(1);
  const std::u16string default2 = DefaultShortName(2);
  const std::u16string default3 = DefaultShortName(3);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", "{not-valid-json"}});

  // Malformed JSON should safely fall back to codebase defaults.
  EXPECT_EQ(GetAiSubscriptionTierShortName(1), default1);
  EXPECT_EQ(GetAiSubscriptionTierShortName(2), default2);
  EXPECT_EQ(GetAiSubscriptionTierShortName(3), default3);
}

TEST_F(AiSubscriptionTierNamesTest, FullNames) {
  const std::u16string tier1 = GetAiSubscriptionTierFullName(1);
  const std::u16string tier2 = GetAiSubscriptionTierFullName(2);
  const std::u16string tier3 = GetAiSubscriptionTierFullName(3);
  EXPECT_FALSE(tier1.empty());
  EXPECT_FALSE(tier2.empty());
  EXPECT_FALSE(tier3.empty());
  EXPECT_NE(tier1, tier2);
  EXPECT_NE(tier1, tier3);
  EXPECT_NE(tier2, tier3);
}

TEST_F(AiSubscriptionTierNamesTest, UnknownTiersHaveEmptyNames) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnabledAiSubscriptionTierNameOverride);

  for (int tier : {-1, 0, 4, 99}) {
    SCOPED_TRACE(tier);
    EXPECT_EQ(GetAiSubscriptionTierShortName(tier), std::u16string());
    EXPECT_EQ(GetAiSubscriptionTierFullName(tier), std::u16string());
  }
}

TEST_F(AiSubscriptionTierNamesTest, FullNamesDifferFromShortNames) {
  EXPECT_NE(GetAiSubscriptionTierFullName(1), DefaultShortName(1));
  EXPECT_NE(GetAiSubscriptionTierFullName(2), DefaultShortName(2));
  EXPECT_NE(GetAiSubscriptionTierFullName(3), DefaultShortName(3));
}

TEST_F(AiSubscriptionTierNamesTest, FullNamesEmptyWhenShortNameOverridden) {
  const std::u16string default3 = DefaultFullName(3);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", R"({"1": "", "2": "Advanced"})"}});

  // Tiers with an overridden short name have no full name.
  EXPECT_EQ(GetAiSubscriptionTierFullName(1), std::u16string());
  EXPECT_EQ(GetAiSubscriptionTierFullName(2), std::u16string());
  // Tiers without an override keep their built-in full name.
  EXPECT_EQ(GetAiSubscriptionTierFullName(3), default3);
}

TEST_F(AiSubscriptionTierNamesTest, FullNamesInvalidJsonFallback) {
  const std::u16string default1 = DefaultFullName(1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kEnabledAiSubscriptionTierNameOverride,
      {{"ai_tier_override_mapping", "{not-valid-json"}});

  // A malformed mapping overrides nothing: keep the built-in full name.
  EXPECT_EQ(GetAiSubscriptionTierFullName(1), default1);
}

}  // namespace
}  // namespace subscription_eligibility
