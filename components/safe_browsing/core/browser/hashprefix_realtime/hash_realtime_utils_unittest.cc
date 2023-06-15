// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5_alpha1.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(HashRealTimeUtilsTest, TestGetHashPrefix) {
  EXPECT_EQ(
      hash_realtime_utils::GetHashPrefix("abcd1111111111111111111111111111"),
      "abcd");
  EXPECT_EQ(
      hash_realtime_utils::GetHashPrefix("dcba1111111111111111111111111111"),
      "dcba");
}

TEST(HashRealTimeUtilsTest, TestIsThreatTypeRelevant) {
  EXPECT_TRUE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::MALWARE));
  EXPECT_TRUE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::SOCIAL_ENGINEERING));
  EXPECT_TRUE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::UNWANTED_SOFTWARE));
  EXPECT_TRUE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::SUSPICIOUS));
  EXPECT_TRUE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::TRICK_TO_BILL));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION));
  EXPECT_FALSE(
      hash_realtime_utils::IsThreatTypeRelevant(V5::ThreatType::API_ABUSE));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::SOCIAL_ENGINEERING_ADS));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION));
  EXPECT_FALSE(hash_realtime_utils::IsThreatTypeRelevant(
      V5::ThreatType::BETTER_ADS_VIOLATION));
}

TEST(HashRealTimeUtilsTest,
     TestIsHashRealTimeLookupEligibleInSession_FeatureOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kHashPrefixRealTimeLookups);
  EXPECT_TRUE(hash_realtime_utils::IsHashRealTimeLookupEligibleInSession());
}
TEST(HashRealTimeUtilsTest,
     TestIsHashRealTimeLookupEligibleInSession_FeatureOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kHashPrefixRealTimeLookups);
  EXPECT_FALSE(hash_realtime_utils::IsHashRealTimeLookupEligibleInSession());
}
TEST(HashRealTimeUtilsTest, TestDetermineHashRealTimeSelection) {
  struct TestCase {
    SafeBrowsingState safe_browsing_state =
        SafeBrowsingState::STANDARD_PROTECTION;
    bool is_off_the_record = false;
    bool is_feature_on = true;
    absl::optional<bool> lookups_allowed_by_policy = absl::nullopt;
    hash_realtime_utils::HashRealTimeSelection expected_selection;
  } test_cases[] = {
#if BUILDFLAG(IS_ANDROID)
    // Lookups disabled for Android.
    {.expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone},
#else
    // HashRealTimeService lookups selected.
    {.expected_selection =
         hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService},
    // Lookups disabled for ESB.
    {.safe_browsing_state = SafeBrowsingState::ENHANCED_PROTECTION,
     .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone},
    // Lookups disabled due to being off the record.
    {.is_off_the_record = true,
     .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone},
    // Lookups disabled because the feature is disabled.
    {.is_feature_on = false,
     .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone},
    // Lookups allowed because policy allows them and nothing else prevents
    // them.
    {.lookups_allowed_by_policy = true,
     .expected_selection =
         hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService},
    // Lookups disabled because policy prevents them.
    {.lookups_allowed_by_policy = false,
     .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone},
#endif
  };

  for (const auto& test_case : test_cases) {
    base::test::ScopedFeatureList scoped_feature_list;
    TestingPrefServiceSimple prefs;
    if (test_case.is_feature_on) {
      scoped_feature_list.InitAndEnableFeature(kHashPrefixRealTimeLookups);
    } else {
      scoped_feature_list.InitAndDisableFeature(kHashPrefixRealTimeLookups);
    }
    RegisterProfilePrefs(prefs.registry());
    SetSafeBrowsingState(&prefs, test_case.safe_browsing_state);
    if (test_case.lookups_allowed_by_policy.has_value()) {
      prefs.SetBoolean(prefs::kHashPrefixRealTimeChecksAllowedByPolicy,
                       test_case.lookups_allowed_by_policy.value());
    }
    EXPECT_EQ(hash_realtime_utils::DetermineHashRealTimeSelection(
                  test_case.is_off_the_record, &prefs),
              test_case.expected_selection);
  }
}

}  // namespace safe_browsing
