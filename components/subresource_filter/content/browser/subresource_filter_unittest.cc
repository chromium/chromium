// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/content_activation_list_utils.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_test_harness.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

class SubresourceFilterTest : public SubresourceFilterTestHarness {};

namespace {

const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions2";

}  // namespace

TEST_F(SubresourceFilterTest, SimpleAllowedLoad) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.test");
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 0);
}

TEST_F(SubresourceFilterTest, SimpleDisallowedLoad) {
  base::HistogramTester histogram_tester;
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 1);
}

TEST_F(SubresourceFilterTest, DeactivateUrl_ChangeSiteActivationToFalse) {
  GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_TRUE(GetSettingsManager()->GetSiteActivationFromMetadata(url));

  RemoveURLFromBlocklist(url);

  // Navigate to |url| again and expect the site's activation to be set
  // to false.
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_FALSE(GetSettingsManager()->GetSiteActivationFromMetadata(url));
}

// If the underlying configuration changes and a site only activates to DRYRUN,
// we should clear the metadata.
TEST_F(SubresourceFilterTest, ActivationToDryRun_ChangeSiteActivationToFalse) {
  GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_TRUE(GetSettingsManager()->GetSiteActivationFromMetadata(url));

  // If the site later activates as DRYRUN due to e.g. a configuration change,
  // it should also be removed from the metadata.
  scoped_configuration().ResetConfiguration(Configuration(
      mojom::ActivationLevel::kDryRun, ActivationScope::ACTIVATION_LIST,
      ActivationList::SUBRESOURCE_FILTER));

  // Navigate to |url| again and expect the site's activation to be set to
  // false.
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_FALSE(GetSettingsManager()->GetSiteActivationFromMetadata(url));
}

TEST_F(SubresourceFilterTest,
       ExplicitAllowlisting_ShouldNotChangeSiteActivation) {
  GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  // Simulate explicit allowlisting and reload.
  GetSettingsManager()->AllowlistSite(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  // Site is still on SB blocklist, activation should stay true.
  EXPECT_TRUE(GetSettingsManager()->GetSiteActivationFromMetadata(url));
}

TEST_F(SubresourceFilterTest, SimpleAllowedLoad_WithObserver) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);

  TestSubresourceFilterObserver observer(web_contents());
  SimulateNavigateAndCommit(url, main_rfh());

  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            observer.GetPageActivation(url).value());

  GURL allowed_url("https://example.test/foo");
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  SimulateNavigateAndCommit(GURL(allowed_url), subframe);
  EXPECT_EQ(LoadPolicy::ALLOW, *observer.GetChildFrameLoadPolicy(allowed_url));
  EXPECT_FALSE(observer.GetIsAdFrame(subframe->GetFrameTreeNodeId()));
}

TEST_F(SubresourceFilterTest, SimpleDisallowedLoad_WithObserver) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);

  TestSubresourceFilterObserver observer(web_contents());
  SimulateNavigateAndCommit(url, main_rfh());

  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            observer.GetPageActivation(url).value());

  GURL disallowed_url(SubresourceFilterTest::kDefaultDisallowedUrl);
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  content::TestNavigationObserver navigation_observer(
      web_contents(), content::MessageLoopRunner::QuitMode::IMMEDIATE,
      false /* ignore_uncommitted_navigations */);
  EXPECT_FALSE(
      SimulateNavigateAndCommit(GURL(kDefaultDisallowedUrl), subframe));
  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(LoadPolicy::DISALLOW,
            *observer.GetChildFrameLoadPolicy(disallowed_url));
  EXPECT_TRUE(observer.GetIsAdFrame(subframe->GetFrameTreeNodeId()));
}

TEST_F(SubresourceFilterTest, RefreshMetadataOnActivation) {
  const GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_TRUE(GetSettingsManager()->GetSiteActivationFromMetadata(url));

  // Allowlist via content settings.
  GetSettingsManager()->AllowlistSite(url);

  // Remove from blocklist, will set metadata activation to false.
  // Note that there is still an exception in content settings.
  RemoveURLFromBlocklist(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(GetSettingsManager()->GetSiteActivationFromMetadata(url));

  // Site re-added to the blocklist. Should not activate due to allowlist, but
  // there should be page info / site details.
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetSettingsManager()->GetSitePermission(url));
  EXPECT_TRUE(GetSettingsManager()->GetSiteActivationFromMetadata(url));
}

enum class AdBlockOnAbusiveSitesTest { kEnabled, kDisabled };

TEST_F(SubresourceFilterTest, NotifySafeBrowsing) {
  typedef safe_browsing::SubresourceFilterType Type;
  typedef safe_browsing::SubresourceFilterLevel Level;
  const struct {
    AdBlockOnAbusiveSitesTest adblock_on_abusive_sites;
    safe_browsing::SubresourceFilterMatch match;
    ActivationList expected_activation;
    bool expected_warning;
  } kTestCases[]{
      // AdBlockOnAbusiveSitesTest::kDisabled
      {AdBlockOnAbusiveSitesTest::kDisabled,
       {},
       ActivationList::SUBRESOURCE_FILTER,
       false},
      {AdBlockOnAbusiveSitesTest::kDisabled,
       {{Type::ABUSIVE, Level::ENFORCE}},
       ActivationList::NONE,
       false},
      {AdBlockOnAbusiveSitesTest::kDisabled,
       {{Type::ABUSIVE, Level::WARN}},
       ActivationList::NONE,
       false},
      {AdBlockOnAbusiveSitesTest::kDisabled,
       {{Type::BETTER_ADS, Level::ENFORCE}},
       ActivationList::BETTER_ADS,
       false},
      {AdBlockOnAbusiveSitesTest::kDisabled,
       {{Type::BETTER_ADS, Level::WARN}},
       ActivationList::BETTER_ADS,
       true},
      {AdBlockOnAbusiveSitesTest::kDisabled,
       {{Type::BETTER_ADS, Level::ENFORCE}, {Type::ABUSIVE, Level::ENFORCE}},
       ActivationList::BETTER_ADS,
       false},
      // AdBlockOnAbusiveSitesTest::kEnabled
      {AdBlockOnAbusiveSitesTest::kEnabled,
       {{Type::ABUSIVE, Level::ENFORCE}},
       ActivationList::ABUSIVE,
       false},
      {AdBlockOnAbusiveSitesTest::kEnabled,
       {{Type::ABUSIVE, Level::WARN}},
       ActivationList::ABUSIVE,
       true}};

  const GURL url("https://example.test");
  for (const auto& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        kFilterAdsOnAbusiveSites, test_case.adblock_on_abusive_sites ==
                                      AdBlockOnAbusiveSitesTest::kEnabled);
    TestSubresourceFilterObserver observer(web_contents());
    auto threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER;
    safe_browsing::ThreatMetadata metadata;
    metadata.subresource_filter_match = test_case.match;
    fake_safe_browsing_database()->AddBlocklistedUrl(url, threat_type,
                                                     metadata);
    SimulateNavigateAndCommit(url, main_rfh());
    bool warning = false;
    EXPECT_EQ(test_case.expected_activation,
              GetListForThreatTypeAndMetadata(threat_type, metadata, &warning));
    EXPECT_EQ(warning, test_case.expected_warning);
  }
}

TEST_F(SubresourceFilterTest, WarningSite_NoMetadata) {
  Configuration config(mojom::ActivationLevel::kEnabled,
                       ActivationScope::ACTIVATION_LIST,
                       ActivationList::BETTER_ADS);
  scoped_configuration().ResetConfiguration(std::move(config));
  const GURL url("https://example.test/");
  safe_browsing::ThreatMetadata metadata;
  metadata.subresource_filter_match
      [safe_browsing::SubresourceFilterType::BETTER_ADS] =
      safe_browsing::SubresourceFilterLevel::WARN;
  auto threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER;
  fake_safe_browsing_database()->AddBlocklistedUrl(url, threat_type, metadata);

  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(GetSettingsManager()->GetSiteActivationFromMetadata(url));
}

}  // namespace subresource_filter
