// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
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

TEST(HashRealTimeUtilsTest, TestCanCheckUrl) {
  auto can_check_url = [](std::string url) {
    EXPECT_TRUE(GURL(url).is_valid());
    return hash_realtime_utils::CanCheckUrl(GURL(url));
  };
  // Yes: HTTPS and main-frame URL.
  EXPECT_TRUE(can_check_url("https://example.test/path"));
  // Yes: HTTP and main-frame URL.
  EXPECT_TRUE(can_check_url("http://example.test/path"));
  // No: The URL scheme is not HTTP/HTTPS.
  EXPECT_FALSE(can_check_url("ftp://example.test/path"));
  // No: It's localhost.
  EXPECT_FALSE(can_check_url("http://localhost/path"));
  // No: The host is an IP address, but is not publicly routable.
  EXPECT_FALSE(can_check_url("http://0.0.0.0"));
  // Yes: The host is an IP address and is publicly routable.
  EXPECT_TRUE(can_check_url("http://1.0.0.0"));
  // No: Hostname does not have at least 1 dot.
  EXPECT_FALSE(can_check_url("https://example/path"));
  // No: Hostname does not have at least 3 characters.
  EXPECT_FALSE(can_check_url("https://e./path"));
}

TEST(HashRealTimeUtilsTest, TestIsHashDetailRelevant) {
  auto create_hash_detail =
      [](V5::ThreatType threat_type,
         std::optional<std::vector<V5::ThreatAttribute>> threat_attributes) {
        V5::FullHash::FullHashDetail detail;
        detail.set_threat_type(threat_type);
        if (threat_attributes.has_value()) {
          for (const auto& attribute : threat_attributes.value()) {
            detail.add_attributes(attribute);
          }
        }
        return detail;
      };
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(
      create_hash_detail(V5::ThreatType::MALWARE, std::nullopt)));
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(
      create_hash_detail(V5::ThreatType::SOCIAL_ENGINEERING, std::nullopt)));
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(
      create_hash_detail(V5::ThreatType::UNWANTED_SOFTWARE, std::nullopt)));
#if BUILDFLAG(IS_IOS)
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::SOCIAL_ENGINEERING,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::CANARY}))));
#else
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::SOCIAL_ENGINEERING,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::CANARY}))));
#endif
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::MALWARE,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::CANARY}))));
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::UNWANTED_SOFTWARE,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::CANARY}))));
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::TRICK_TO_BILL,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::CANARY}))));
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::SOCIAL_ENGINEERING,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::FRAME_ONLY}))));
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::MALWARE,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::FRAME_ONLY}))));
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::UNWANTED_SOFTWARE,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::FRAME_ONLY}))));
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::TRICK_TO_BILL,
      std::vector<V5::ThreatAttribute>({V5::ThreatAttribute::FRAME_ONLY}))));
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::SOCIAL_ENGINEERING,
      std::vector<V5::ThreatAttribute>(
          {V5::ThreatAttribute::CANARY, V5::ThreatAttribute::FRAME_ONLY}))));
  EXPECT_TRUE(hash_realtime_utils::IsHashDetailRelevant(
      create_hash_detail(V5::ThreatType::TRICK_TO_BILL, std::nullopt)));
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION, std::nullopt)));
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(
      create_hash_detail(V5::ThreatType::API_ABUSE, std::nullopt)));
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(create_hash_detail(
      V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION, std::nullopt)));
  EXPECT_FALSE(hash_realtime_utils::IsHashDetailRelevant(
      create_hash_detail(V5::ThreatType::BETTER_ADS_VIOLATION, std::nullopt)));
}

TEST(HashRealTimeUtilsTest, TestIsHashRealTimeLookupEligibleInSession_Yes) {
  hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kHashPrefixRealTimeLookups);
  EXPECT_TRUE(hash_realtime_utils::IsHashRealTimeLookupEligibleInSession());
}
TEST(HashRealTimeUtilsTest,
     TestIsHashRealTimeLookupEligibleInSession_FeatureOff) {
  hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kHashPrefixRealTimeLookups);
  EXPECT_FALSE(hash_realtime_utils::IsHashRealTimeLookupEligibleInSession());
}
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST(HashRealTimeUtilsTest,
     TestIsHashRealTimeLookupEligibleInSession_GoogleChromeBrandingOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kHashPrefixRealTimeLookups);
  EXPECT_FALSE(hash_realtime_utils::IsHashRealTimeLookupEligibleInSession());
}
#endif

TEST(HashRealTimeUtilsTest,
     TestIsHashRealTimeLookupEligibleInSessionAndLocation) {
  struct TestCase {
    bool is_feature_on = true;
    bool has_google_chrome_branding = true;
    std::optional<std::string> latest_country = std::nullopt;
    bool expected_eligibility;
  } test_cases[] = {
      // Lookups eligible when no country is provided.
      {.expected_eligibility = true},
      // Lookups eligible for US.
      {.latest_country = "us", .expected_eligibility = true},
      // Lookups ineligible for CN.
      {.latest_country = "cn", .expected_eligibility = false},
      // Lookups ineligible when the feature is disabled.
      {.is_feature_on = false, .expected_eligibility = false},
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // Lookups ineligible because it's not a branded build.
      {.has_google_chrome_branding = false, .expected_eligibility = false},
#endif
  };

  for (const auto& test_case : test_cases) {
    hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding;
    base::test::ScopedFeatureList scoped_feature_list;
    if (test_case.is_feature_on) {
      scoped_feature_list.InitAndEnableFeature(kHashPrefixRealTimeLookups);
    } else {
      scoped_feature_list.InitAndDisableFeature(kHashPrefixRealTimeLookups);
    }
    if (!test_case.has_google_chrome_branding) {
      apply_branding.StopApplyingBranding();
    }
    EXPECT_EQ(
        hash_realtime_utils::IsHashRealTimeLookupEligibleInSessionAndLocation(
            test_case.latest_country),
        test_case.expected_eligibility);
  }
}

TEST(HashRealTimeUtilsTest, TestDetermineHashRealTimeSelection) {
  hash_realtime_utils::HashRealTimeSelection enabled_selection =
#if BUILDFLAG(IS_ANDROID)
      hash_realtime_utils::HashRealTimeSelection::kDatabaseManager;
#else
      hash_realtime_utils::HashRealTimeSelection::kHashRealTimeService;
#endif
  struct TestCase {
    SafeBrowsingState safe_browsing_state =
        SafeBrowsingState::STANDARD_PROTECTION;
    bool is_off_the_record = false;
    bool is_feature_on = true;
    bool has_google_chrome_branding = true;
    std::optional<std::string> latest_country = std::nullopt;
    std::optional<bool> lookups_allowed_by_policy = std::nullopt;
    hash_realtime_utils::HashRealTimeSelection expected_selection;
    bool expected_log_usage_histograms = true;
    bool expected_ineligible_for_session_or_location_log = false;
    bool expected_off_the_record_log = false;
    bool expected_not_standard_protection_log = false;
    bool expected_not_allowed_by_policy_log = false;
    bool expected_no_google_chrome_branding_log = false;
    bool expected_feature_off_log = false;
    bool expected_ineligible_for_location_log = false;
  } test_cases[] = {
      {.expected_selection = enabled_selection},
      // Lookups disabled for ESB.
      {.safe_browsing_state = SafeBrowsingState::ENHANCED_PROTECTION,
       .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone,
       .expected_not_standard_protection_log = true},
      // Lookups disabled due to being off the record.
      {.is_off_the_record = true,
       .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone,
       .expected_off_the_record_log = true},
      // Lookups disabled because the feature is disabled.
      {.is_feature_on = false,
       .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone,
       .expected_ineligible_for_session_or_location_log = true,
       .expected_feature_off_log = true},
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // Lookups disabled because it's not a branded build.
      {.has_google_chrome_branding = false,
       .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone,
       .expected_ineligible_for_session_or_location_log = true,
       .expected_no_google_chrome_branding_log = true},
#endif
      // Lookups allowed for US.
      {.latest_country = "us", .expected_selection = enabled_selection},
      // Lookups disabled for CN.
      {.latest_country = "cn",
       .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone,
       .expected_ineligible_for_session_or_location_log = true,
       .expected_ineligible_for_location_log = true},
      // Lookups allowed because policy allows them and nothing else prevents
      // them.
      {.lookups_allowed_by_policy = true,
       .expected_selection = enabled_selection},
      // Lookups disabled because policy prevents them.
      {.lookups_allowed_by_policy = false,
       .expected_selection = hash_realtime_utils::HashRealTimeSelection::kNone,
       .expected_not_allowed_by_policy_log = true},
  };

  auto check_usage_histograms = [](const base::HistogramTester&
                                       histogram_tester,
                                   const TestCase& test_case) {
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.IneligibleForSessionOrLocation",
        /*sample=*/test_case.expected_ineligible_for_session_or_location_log,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.OffTheRecord",
        /*sample=*/test_case.expected_off_the_record_log,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.NotStandardProtection",
        /*sample=*/test_case.expected_not_standard_protection_log,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.NotAllowedByPolicy",
        /*sample=*/test_case.expected_not_allowed_by_policy_log,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.NoGoogleChromeBranding",
        /*sample=*/test_case.expected_no_google_chrome_branding_log,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.FeatureOff",
        /*sample=*/test_case.expected_feature_off_log,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.IneligibleForLocation",
        /*sample=*/test_case.expected_ineligible_for_location_log,
        /*expected_bucket_count=*/1);
  };
  auto check_no_usage_histograms = [](const base::HistogramTester&
                                          histogram_tester) {
    histogram_tester.ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.IneligibleForSessionOrLocation",
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.OffTheRecord",
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.NotStandardProtection",
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.NotAllowedByPolicy",
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.NoGoogleChromeBranding",
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.FeatureOff",
        /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/"SafeBrowsing.HPRT.Ineligible.IneligibleForLocation",
        /*expected_count=*/0);
  };

  for (const auto& test_case : test_cases) {
    hash_realtime_utils::GoogleChromeBrandingPretenderForTesting apply_branding;
    base::test::ScopedFeatureList scoped_feature_list;
    TestingPrefServiceSimple prefs;
    base::HistogramTester histogram_tester;
    if (test_case.is_feature_on) {
      scoped_feature_list.InitAndEnableFeature(kHashPrefixRealTimeLookups);
    } else {
      scoped_feature_list.InitAndDisableFeature(kHashPrefixRealTimeLookups);
    }
    if (!test_case.has_google_chrome_branding) {
      apply_branding.StopApplyingBranding();
    }
    RegisterProfilePrefs(prefs.registry());
    SetSafeBrowsingState(&prefs, test_case.safe_browsing_state);
    if (test_case.lookups_allowed_by_policy.has_value()) {
      prefs.SetBoolean(prefs::kHashPrefixRealTimeChecksAllowedByPolicy,
                       test_case.lookups_allowed_by_policy.value());
    }
    // Confirm result is correct and no histograms are logged.
    EXPECT_EQ(
        hash_realtime_utils::DetermineHashRealTimeSelection(
            test_case.is_off_the_record, &prefs, test_case.latest_country),
        test_case.expected_selection);
    check_no_usage_histograms(histogram_tester);
    // Confirm result is still correct and relevant histograms are logged.
    EXPECT_EQ(hash_realtime_utils::DetermineHashRealTimeSelection(
                  test_case.is_off_the_record, &prefs, test_case.latest_country,
                  /*log_usage_histograms=*/true),
              test_case.expected_selection);
    if (test_case.expected_log_usage_histograms) {
      check_usage_histograms(histogram_tester, test_case);
    } else {
      check_no_usage_histograms(histogram_tester);
    }
  }
}

}  // namespace safe_browsing
