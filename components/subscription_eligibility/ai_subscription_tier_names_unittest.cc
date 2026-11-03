// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/ai_subscription_tier_names.h"

#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subscription_eligibility {

// Readable gtest failure messages.
void PrintTo(const AiSubscriptionTierDisplayInfo& display_info,
             std::ostream* os) {
  *os << "{show_decoration=" << display_info.show_decoration
      << ", badge_label=\"" << base::UTF16ToUTF8(display_info.badge_label)
      << "\", full_membership_label=\""
      << base::UTF16ToUTF8(display_info.full_membership_label) << "\"}";
}

namespace {

// Returns the default presentation of `tier`. The built-in names differ between
// Google Chrome and Chromium builds, so the tests compare against them instead
// of hardcoding the names.
AiSubscriptionTierDisplayInfo DefaultDisplayInfo(int tier) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAiSubscriptionTierOverrides);
  ResetAiTierOverridesForTesting();
  AiSubscriptionTierDisplayInfo display_info =
      GetAiSubscriptionTierDisplayInfo(tier);
  // Restore the cache for the caller's feature state.
  feature_list.Reset();
  ResetAiTierOverridesForTesting();
  return display_info;
}

// Returns the built-in short name of `tier`.
std::u16string DefaultShortName(int tier) {
  return DefaultDisplayInfo(tier).badge_label;
}

// Returns the built-in full name of `tier`.
std::u16string DefaultFullName(int tier) {
  return DefaultDisplayInfo(tier).full_membership_label;
}

// Expected presentation of a tier that is not displayed at all.
AiSubscriptionTierDisplayInfo NotDisplayedInfo() {
  return {};
}

// Expected presentation of a displayed tier with the given names.
AiSubscriptionTierDisplayInfo DisplayedInfoWithNames(
    std::u16string badge_label,
    std::u16string full_membership_label) {
  return {.show_decoration = true,
          .badge_label = std::move(badge_label),
          .full_membership_label = std::move(full_membership_label)};
}

// Tests run with `kAiSubscriptionTierOverrides` disabled, unless they call
// `SetOverrides()`.
class AiSubscriptionTierNamesTest : public testing::Test {
 protected:
  void SetUp() override { ResetAiTierOverridesForTesting(); }
  void TearDown() override {
    feature_list_.Reset();
    ResetAiTierOverridesForTesting();
  }

  // Enables `kAiSubscriptionTierOverrides` with `overrides` as the value of
  // `kAiTierOverrides`.
  void SetOverrides(const std::string& overrides) {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        kAiSubscriptionTierOverrides, {{"ai_tier_overrides", overrides}});
    ResetAiTierOverridesForTesting();
  }

  // Returns the short name of `tier`, taking the overrides into account.
  static std::u16string ShortName(int tier) {
    return GetAiSubscriptionTierDisplayInfo(tier).badge_label;
  }

  // Returns the full name of `tier`, taking the overrides into account.
  static std::u16string FullName(int tier) {
    return GetAiSubscriptionTierDisplayInfo(tier).full_membership_label;
  }

  // Expects every tier in `tiers` to be unaffected by the overrides, i.e. to
  // have its default presentation.
  static void ExpectTiersUseDefaultDisplayInfo(std::vector<int> tiers) {
    for (int tier : tiers) {
      EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(tier),
                DefaultDisplayInfo(tier))
          << "tier " << tier;
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AiSubscriptionTierNamesTest, DefaultShortNamesWhenFeatureDisabled) {
  const std::u16string tier1 = ShortName(1);
  const std::u16string tier2 = ShortName(2);
  const std::u16string tier3 = ShortName(3);
  EXPECT_FALSE(tier1.empty());
  EXPECT_FALSE(tier2.empty());
  EXPECT_FALSE(tier3.empty());
  EXPECT_NE(tier1, tier2);
  EXPECT_NE(tier1, tier3);
  EXPECT_NE(tier2, tier3);
}

TEST_F(AiSubscriptionTierNamesTest, FullShortNameOverride) {
  SetOverrides(R"({"tier_names": {
      "1": {"short_name": "Custom 1", "full_name": "AI Custom 1"},
      "2": {"short_name": "Custom 2", "full_name": "AI Custom 2"},
      "3": {"short_name": "Custom 3", "full_name": "AI Custom 3"}}})");

  EXPECT_EQ(ShortName(1), u"Custom 1");
  EXPECT_EQ(ShortName(2), u"Custom 2");
  EXPECT_EQ(ShortName(3), u"Custom 3");
  EXPECT_EQ(ShortName(99), std::u16string());
}

TEST_F(AiSubscriptionTierNamesTest, PartialShortNameOverrideWithFallback) {
  SetOverrides(R"({"tier_names": {
      "2": {"short_name": "Advanced", "full_name": "AI Advanced"}}})");

  // Tier 1 and 3 should fall back to codebase defaults.
  EXPECT_EQ(ShortName(1), DefaultShortName(1));
  EXPECT_EQ(ShortName(2), u"Advanced");
  EXPECT_EQ(ShortName(3), DefaultShortName(3));
}

TEST_F(AiSubscriptionTierNamesTest, OverrideCanNameUnknownTier) {
  SetOverrides(R"({"tier_names": {
      "4": {"short_name": "Max", "full_name": "AI Max"}}})");

  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(4),
            DisplayedInfoWithNames(u"Max", u"AI Max"));
}

TEST_F(AiSubscriptionTierNamesTest, ExplicitEmptyString) {
  SetOverrides(R"({"tier_names": {
      "1": {"short_name": "", "full_name": ""},
      "2": {"short_name": "Advanced", "full_name": "AI Advanced"},
      "4": {"short_name": "Max", "full_name": ""},
      "5": {"short_name": "", "full_name": "AI Five"}}})");

  // Explicit empty strings hide the badge text, respectively use the generic
  // a11y string, but the tier is still displayed.
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(1),
            DisplayedInfoWithNames(std::u16string(), std::u16string()));
  // Overridden name.
  EXPECT_EQ(ShortName(2), u"Advanced");
  // Non-overriden tier.
  EXPECT_EQ(ShortName(3), DefaultShortName(3));
  // Each name can be empty on its own.
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(4),
            DisplayedInfoWithNames(u"Max", u""));
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(5),
            DisplayedInfoWithNames(u"", u"AI Five"));
}

TEST_F(AiSubscriptionTierNamesTest, InvalidJsonFallback) {
  SetOverrides("{not-valid-json");

  // Malformed JSON should safely fall back to codebase defaults.
  EXPECT_EQ(ShortName(1), DefaultShortName(1));
  EXPECT_EQ(ShortName(2), DefaultShortName(2));
  EXPECT_EQ(ShortName(3), DefaultShortName(3));
}

TEST_F(AiSubscriptionTierNamesTest, FullNames) {
  const std::u16string tier1 = FullName(1);
  const std::u16string tier2 = FullName(2);
  const std::u16string tier3 = FullName(3);
  EXPECT_FALSE(tier1.empty());
  EXPECT_FALSE(tier2.empty());
  EXPECT_FALSE(tier3.empty());
  EXPECT_NE(tier1, tier2);
  EXPECT_NE(tier1, tier3);
  EXPECT_NE(tier2, tier3);
}

TEST_F(AiSubscriptionTierNamesTest, UnknownTiersHaveEmptyNames) {
  // No subscription: not displayed.
  for (int tier : {-1, 0}) {
    SCOPED_TRACE(tier);
    EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(tier), NotDisplayedInfo());
  }
  // Tiers unknown to the binary: displayed, without names.
  for (int tier : {4, 99}) {
    SCOPED_TRACE(tier);
    EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(tier),
              DisplayedInfoWithNames(u"", u""));
  }
}

TEST_F(AiSubscriptionTierNamesTest, FullNamesDifferFromShortNames) {
  EXPECT_NE(FullName(1), DefaultShortName(1));
  EXPECT_NE(FullName(2), DefaultShortName(2));
  EXPECT_NE(FullName(3), DefaultShortName(3));
}

TEST_F(AiSubscriptionTierNamesTest, FullNamesInvalidJsonFallback) {
  SetOverrides("{not-valid-json");

  // A malformed value overrides nothing: keep the default full name.
  EXPECT_EQ(FullName(1), DefaultFullName(1));
}

TEST_F(AiSubscriptionTierNamesTest, EmptyOverridesKeepDefault) {
  // Feature enabled with the default param value.
  SetOverrides(kAiTierOverrides.default_value);
  ExpectTiersUseDefaultDisplayInfo({1, 2, 3, 4});
  for (const char* overrides :
       {"{}", R"({"tier_names": {}, "excluded_tiers": []})",
        // Param of the removed `EnabledAiSubscriptionTierNameOverride`
        // feature: not a key of the new value, ignored.
        R"({"1": "Custom 1"})"}) {
    SCOPED_TRACE(overrides);
    SetOverrides(overrides);
    ExpectTiersUseDefaultDisplayInfo({1, 2, 3, 4});
  }
}

TEST_F(AiSubscriptionTierNamesTest, ExcludedTiers) {
  // Duplicates are harmless.
  SetOverrides(R"({"excluded_tiers": [2, 99, 2]})");

  // Tier with default names and tier unknown to the binary.
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(2), NotDisplayedInfo());
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(99), NotDisplayedInfo());
  // Not listed.
  ExpectTiersUseDefaultDisplayInfo({1, 3, 4});
}

// A tier both named and excluded is excluded. The rest of the value still
// applies.
TEST_F(AiSubscriptionTierNamesTest, ExclusionWinsOverTierNames) {
  SetOverrides(R"({
      "tier_names": {
        "1": {"short_name": "One", "full_name": "AI One"},
        "2": {"short_name": "Super", "full_name": "AI Super"}
      },
      "excluded_tiers": [2]
    })");

  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(2), NotDisplayedInfo());
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(1),
            DisplayedInfoWithNames(u"One", u"AI One"));
}

TEST_F(AiSubscriptionTierNamesTest, UnknownAndDuplicateKeys) {
  // Unknown keys are ignored.
  SetOverrides(R"({
      "tier_names": {
        "2": {"short_name": "Super", "full_name": "AI Super", "icon": 1}
      },
      "excluded_tiers": [3],
      "future_key": {"anything": true}
    })");
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(2),
            DisplayedInfoWithNames(u"Super", u"AI Super"));
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(3), NotDisplayedInfo());

  // Duplicate keys can't be detected: the last one silently wins.
  SetOverrides(R"({
      "excluded_tiers": [2],
      "tier_names": {
        "2": {"short_name": "First", "full_name": "AI First"},
        "2": {"short_name": "Super", "full_name": "AI Super"}
      },
      "excluded_tiers": [3]
    })");
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(2),
            DisplayedInfoWithNames(u"Super", u"AI Super"));
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(3), NotDisplayedInfo());
}

// Tiers <= 0 mean no subscription: overrides for them are valid, so they don't
// invalidate the rest of the value, but have no effect.
TEST_F(AiSubscriptionTierNamesTest,
       OverridesForTiersWithoutSubscriptionIgnored) {
  SetOverrides(R"({
    "tier_names": {
      "0": {"short_name": "Zero", "full_name": "AI Zero"},
      "-1": {"short_name": "Minus", "full_name": "AI Minus"},
      "2": {"short_name": "Super", "full_name": "AI Super"}
    },
    "excluded_tiers": [0, -1, 3]
  })");

  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(0), NotDisplayedInfo());
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(-1), NotDisplayedInfo());
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(2),
            DisplayedInfoWithNames(u"Super", u"AI Super"));
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(3), NotDisplayedInfo());
}

// The parsed `kAiTierOverrides` is cached: changing the feature state has no
// effect until `ResetAiTierOverridesForTesting()` is called.
TEST_F(AiSubscriptionTierNamesTest, OverridesAreCached) {
  SetOverrides(R"({"excluded_tiers": [2]})");
  ASSERT_EQ(GetAiSubscriptionTierDisplayInfo(2), NotDisplayedInfo());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAiSubscriptionTierOverrides);
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(2), NotDisplayedInfo());

  ResetAiTierOverridesForTesting();
  EXPECT_TRUE(GetAiSubscriptionTierDisplayInfo(2).show_decoration);
}

// Valid parts of the values used by
// `AiSubscriptionTierNamesInvalidOverridesTest` below: a rename of tier 1 and
// an exclusion of tier 3. The values refer to them as `$1` and `$2`, see
// `ExpandOverrides()`.
constexpr char kValidTierNamesPart[] =
    R"("1": {"short_name": "One", "full_name": "One"})";
constexpr char kValidExcludedTiersPart[] = "3";
constexpr char kValidOverrides[] =
    R"({"tier_names": {$1}, "excluded_tiers": [$2]})";

// Replaces `$1` and `$2` in `overrides` by the valid parts above.
std::string ExpandOverrides(const std::string& overrides) {
  return base::ReplaceStringPlaceholders(
      overrides, {kValidTierNamesPart, kValidExcludedTiersPart},
      /*offsets=*/nullptr);
}

// Sanity check for `AiSubscriptionTierNamesInvalidOverridesTest` below:
// without an invalid part, the valid parts apply.
TEST_F(AiSubscriptionTierNamesTest, ValidPartsOfInvalidOverridesApply) {
  SetOverrides(ExpandOverrides(kValidOverrides));
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(1),
            DisplayedInfoWithNames(u"One", u"One"));
  EXPECT_EQ(GetAiSubscriptionTierDisplayInfo(3), NotDisplayedInfo());
}

// A value with a single invalid part is ignored entirely: even its valid parts
// don't apply.
struct InvalidOverridesTestCase {
  const char* test_name;
  // Expanded by `ExpandOverrides()`.
  const char* overrides;
};

class AiSubscriptionTierNamesInvalidOverridesTest
    : public AiSubscriptionTierNamesTest,
      public testing::WithParamInterface<InvalidOverridesTestCase> {};

TEST_P(AiSubscriptionTierNamesInvalidOverridesTest, IgnoredEntirely) {
  SetOverrides(ExpandOverrides(GetParam().overrides));
  ExpectTiersUseDefaultDisplayInfo({1, 2, 3, 4});
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AiSubscriptionTierNamesInvalidOverridesTest,
    testing::Values(
        // Not a JSON dictionary.
        InvalidOverridesTestCase{"EmptyString", ""},
        InvalidOverridesTestCase{"MalformedJson", "{not-valid-json"},
        InvalidOverridesTestCase{"NotADictionary", "[]"},
        InvalidOverridesTestCase{
            "TrailingComma",
            R"({"tier_names": {$1}, "excluded_tiers": [$2],})"},
        // Wrong container types.
        InvalidOverridesTestCase{
            "TierNamesNotADictionary",
            R"({"tier_names": [], "excluded_tiers": [$2]})"},
        InvalidOverridesTestCase{
            "ExcludedTiersNotAList",
            R"({"tier_names": {$1}, "excluded_tiers": $2})"},
        // Invalid tier keys.
        InvalidOverridesTestCase{
            "TierKeyWithLeadingZero",
            R"({"tier_names": {$1, "02": {"short_name": "A", "full_name": "B"}},
                "excluded_tiers": [$2]})"},
        InvalidOverridesTestCase{
            "TierKeyWithPlusSign",
            R"({"tier_names": {$1, "+2": {"short_name": "A", "full_name": "B"}},
                "excluded_tiers": [$2]})"},
        InvalidOverridesTestCase{"TierKeyNotANumber",
                                 R"({"tier_names": {$1,
                               "two": {"short_name": "A", "full_name": "B"}},
                "excluded_tiers": [$2]})"},
        InvalidOverridesTestCase{"TierKeyOutOfRange",
                                 R"({"tier_names": {$1,
                               "99999999999": {"short_name": "A",
                                               "full_name": "B"}},
                "excluded_tiers": [$2]})"},
        // Invalid names.
        InvalidOverridesTestCase{
            "NamesNotADictionary",
            R"({"tier_names": {$1, "2": "Super"}, "excluded_tiers": [$2]})"},
        InvalidOverridesTestCase{
            "MissingFullName",
            R"({"tier_names": {$1, "2": {"short_name": "Super"}},
                "excluded_tiers": [$2]})"},
        InvalidOverridesTestCase{
            "MissingShortName",
            R"({"tier_names": {$1, "2": {"full_name": "AI Super"}},
                "excluded_tiers": [$2]})"},
        InvalidOverridesTestCase{"NameNotAString",
                                 R"({"tier_names": {$1,
                               "2": {"short_name": 2, "full_name": "Super"}},
                "excluded_tiers": [$2]})"},
        // Exclusion doesn't make the invalid names of a tier valid.
        InvalidOverridesTestCase{
            "InvalidNamesOfExcludedTier",
            R"({"tier_names": {$1, "3": {"short_name": "Three"}},
                "excluded_tiers": [$2]})"},
        // Invalid excluded tiers.
        InvalidOverridesTestCase{
            "ExcludedTierNotAnInteger",
            R"({"tier_names": {$1}, "excluded_tiers": [$2, "4"]})"},
        InvalidOverridesTestCase{
            "ExcludedTierNotAWholeNumber",
            R"({"tier_names": {$1}, "excluded_tiers": [$2, 4.5]})"}),
    [](const testing::TestParamInfo<InvalidOverridesTestCase>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace subscription_eligibility
