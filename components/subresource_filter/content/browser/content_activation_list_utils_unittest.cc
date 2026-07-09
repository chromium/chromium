// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_activation_list_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

enum class AdBlockOnAbusiveSitesTest { kEnabled, kDisabled };

TEST(ContentActivationListUtilsTest, GetListForThreatTypeAndMetadata) {
  using enum safe_browsing::SBThreatType;

  typedef safe_browsing::SubresourceFilterType Type;
  typedef safe_browsing::SubresourceFilterLevel Level;
  const struct {
    std::string test_id;
    safe_browsing::SBThreatType sb_threat_type;
    safe_browsing::SubresourceFilterMatch subresource_filter_match;
    AdBlockOnAbusiveSitesTest adblock_on_abusive_sites;
    ActivationList expected_activation_list;
    bool expected_warning;
  } kTestCases[]{
      {"Phishing_Without_SocialEngineeringAds",
       SB_THREAT_TYPE_URL_PHISHING,
       {},
       AdBlockOnAbusiveSitesTest::kDisabled,
       ActivationList::PHISHING_INTERSTITIAL,
       false},
      {"Empty_SubresourceFilterMatch",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {},
       AdBlockOnAbusiveSitesTest::kDisabled,
       ActivationList::SUBRESOURCE_FILTER,
       false},
      {"BetterAds_Warn_DisableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::BETTER_ADS, Level::WARN}},
       AdBlockOnAbusiveSitesTest::kDisabled,
       ActivationList::BETTER_ADS,
       true},
      {"BetterAds_Enforce_DisableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::BETTER_ADS, Level::ENFORCE}},
       AdBlockOnAbusiveSitesTest::kDisabled,
       ActivationList::BETTER_ADS,
       false},
      {"Abusive_Warn_DisableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::WARN}},
       AdBlockOnAbusiveSitesTest::kDisabled,
       ActivationList::NONE,
       false},
      {"Abusive_Enforce_DisableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::ENFORCE}},
       AdBlockOnAbusiveSitesTest::kDisabled,
       ActivationList::NONE,
       false},
      {"BetterAds_Warn_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::BETTER_ADS, Level::WARN}},
       AdBlockOnAbusiveSitesTest::kEnabled,
       ActivationList::BETTER_ADS,
       true},
      {"BetterAds_Enforce_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::BETTER_ADS, Level::ENFORCE}},
       AdBlockOnAbusiveSitesTest::kEnabled,
       ActivationList::BETTER_ADS,
       false},
      {"Abusive_Warn_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::WARN}},
       AdBlockOnAbusiveSitesTest::kEnabled,
       ActivationList::ABUSIVE,
       true},
      {"Abusive_Enforce_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::ENFORCE}},
       AdBlockOnAbusiveSitesTest::kEnabled,
       ActivationList::ABUSIVE,
       false},
      {"BetterAds_Warn_Abusive_Warn_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::WARN}, {Type::BETTER_ADS, Level::WARN}},
       AdBlockOnAbusiveSitesTest::kDisabled,
       ActivationList::BETTER_ADS,
       true},
      {"BetterAds_Warn_Abusive_Enforce_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::ENFORCE}, {Type::BETTER_ADS, Level::WARN}},
       AdBlockOnAbusiveSitesTest::kEnabled,
       ActivationList::ABUSIVE,
       false},
      {"BetterAds_Enforce_Abusive_Warn_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::WARN}, {Type::BETTER_ADS, Level::ENFORCE}},
       AdBlockOnAbusiveSitesTest::kEnabled,
       ActivationList::BETTER_ADS,
       false},
      {"BetterAds_Enforce_Abusive_Enforce_EnableAdBlockOnAbusiveSites",
       SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       {{Type::ABUSIVE, Level::ENFORCE}, {Type::BETTER_ADS, Level::ENFORCE}},
       AdBlockOnAbusiveSitesTest::kEnabled,
       ActivationList::BETTER_ADS,
       false}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message() << "test_id = " << test_case.test_id);
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        kFilterAdsOnAbusiveSites, test_case.adblock_on_abusive_sites ==
                                      AdBlockOnAbusiveSitesTest::kEnabled);
    bool warning = false;
    EXPECT_EQ(test_case.expected_activation_list,
              GetListForThreatTypeAndMetadata(
                  test_case.sb_threat_type, test_case.subresource_filter_match,
                  &warning));
    EXPECT_EQ(test_case.expected_warning, warning);
  }
}

}  // namespace subresource_filter
