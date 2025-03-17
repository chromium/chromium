// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"

#include <gmock/gmock.h>

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_observer.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_breakage_exception.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom-shared.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}  // namespace content

class PrefService;

namespace privacy_sandbox {
class TrackingProtectionSettings;
}  // namespace privacy_sandbox

namespace subresource_filter {
class VerifiedRulesetDealer;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {
namespace {

using ::content::WebContents;
using ::privacy_sandbox::TrackingProtectionSettings;
using ::subresource_filter::VerifiedRulesetDealer;
using ::subresource_filter::mojom::ActivationLevel;
using ::testing::_;
using ::testing::Return;

// TODO(https://crbug.com/366515692): Add unit tests for other functions, i.e.
// GetThrottleManager, DidFinishNavigation, etc.

struct CreateForWebContentsTestCase {
  std::string test_name;
  bool is_regular_feature_enabled = false;
  bool is_incognito_feature_enabled = false;
  bool is_incognito_profile = false;
  bool nullptr_expected;
};

class CreateForWebContentsHelperTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<CreateForWebContentsTestCase> {
 public:
  CreateForWebContentsHelperTest() = default;

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    scoped_feature_list_.Reset();
  }

  void SetFeatureFlags(bool is_regular_feature_enabled,
                       bool is_incognito_feature_enabled) {
    if (is_regular_feature_enabled && is_incognito_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kEnableFingerprintingProtectionFilter,
           features::kEnableFingerprintingProtectionFilterInIncognito},
          /*disabled_features=*/{});
    } else if (!is_regular_feature_enabled && !is_incognito_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              features::kEnableFingerprintingProtectionFilter,
              features::kEnableFingerprintingProtectionFilterInIncognito});
    } else if (is_regular_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::
                                    kEnableFingerprintingProtectionFilter},
          /*disabled_features=*/{
              features::kEnableFingerprintingProtectionFilterInIncognito});
    } else if (is_incognito_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kEnableFingerprintingProtectionFilterInIncognito},
          /*disabled_features=*/{
              features::kEnableFingerprintingProtectionFilter});
    }
  }

  void ExpectNullptr(
      bool expect_nullptr,
      FingerprintingProtectionWebContentsHelper* web_contents_helper) {
    if (expect_nullptr) {
      EXPECT_EQ(nullptr, web_contents_helper);
    } else {
      EXPECT_NE(nullptr, web_contents_helper);
    }
  }

 protected:
  TestSupport test_support_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const CreateForWebContentsTestCase kTestCases[] = {
    {
        .test_name = "Created_FeaturesOn_RegularProfile",
        .is_regular_feature_enabled = true,
        .is_incognito_feature_enabled = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "Created_FeaturesOn_IncognitoProfile",
        .is_regular_feature_enabled = true,
        .is_incognito_feature_enabled = true,
        .is_incognito_profile = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "Created_RegularFeatureEnabled_RegularProfile",
        .is_regular_feature_enabled = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "Created_IncognitoFeatureEnabled_IncognitoProfile",
        .is_incognito_feature_enabled = true,
        .is_incognito_profile = true,
        .nullptr_expected = false,
    },
    {
        .test_name = "NotCreated_FeaturesOff",
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_RegularFeatureEnabled_IncognitoProfile",
        .is_regular_feature_enabled = true,
        .is_incognito_profile = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_RegularFeatureDisabled_RegularProfile",
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_RegularFeatureDisabled_IncognitoProfile",
        .is_incognito_profile = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_IncognitoFeatureEnabled_RegularProfile",
        .is_incognito_feature_enabled = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_IncognitoFeatureDisabled_IncognitoProfile",
        .is_incognito_profile = true,
        .nullptr_expected = true,
    },
    {
        .test_name = "NotCreated_IncognitoFeatureDisabled_RegularProfile",
        .nullptr_expected = true,
    }};

INSTANTIATE_TEST_SUITE_P(
    CreateForWebContentsHelperTestSuiteInstantiation,
    CreateForWebContentsHelperTest,
    testing::ValuesIn<CreateForWebContentsTestCase>(kTestCases),
    [](const testing::TestParamInfo<CreateForWebContentsHelperTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(CreateForWebContentsHelperTest, CreateForWebContents) {
  const CreateForWebContentsTestCase& test_case = GetParam();

  SetFeatureFlags(test_case.is_regular_feature_enabled,
                  test_case.is_incognito_feature_enabled);

  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.content_settings(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/test_case.is_incognito_profile);

  ExpectNullptr(test_case.nullptr_expected,
                FingerprintingProtectionWebContentsHelper::FromWebContents(
                    RenderViewHostTestHarness::web_contents()));
}

class MockFingerprintingProtectionObserver
    : public FingerprintingProtectionObserver {
 public:
  MOCK_METHOD(void, OnSubresourceBlocked, (), (override));
};

class FingerprintingProtectionNotifyOnBlockedSubresourceTest
    : public content::RenderViewHostTestHarness {
 public:
  FingerprintingProtectionNotifyOnBlockedSubresourceTest() = default;

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    scoped_feature_list_.Reset();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSupport test_support_;
};

TEST_F(FingerprintingProtectionNotifyOnBlockedSubresourceTest,
       OnSubresourceBlockedCalled_NotifyOnBlockedSubresource) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.content_settings(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/false);

  auto* test_web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          RenderViewHostTestHarness::web_contents());
  MockFingerprintingProtectionObserver observer;

  test_web_contents_helper->AddObserver(&observer);

  EXPECT_CALL(observer, OnSubresourceBlocked());
  test_web_contents_helper->NotifyOnBlockedSubresource(
      ActivationLevel::kEnabled);
}

TEST_F(FingerprintingProtectionNotifyOnBlockedSubresourceTest,
       OnSubresourceBlockedNotCalled_WithoutNotifyOnBlockedSubresource) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.content_settings(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/false);

  auto* test_web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          RenderViewHostTestHarness::web_contents());
  MockFingerprintingProtectionObserver observer;

  test_web_contents_helper->AddObserver(&observer);

  // Expect OnSubresourceBlocked is not called without
  // NotifyOnBlockedSubresource called.
  EXPECT_CALL(observer, OnSubresourceBlocked()).Times(0);
}

TEST_F(
    FingerprintingProtectionNotifyOnBlockedSubresourceTest,
    SubresourceBlockedDirtyBit_SetOnBlockAndResetOnNavigation_ActivationEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.content_settings(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/false);

  auto* test_web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          RenderViewHostTestHarness::web_contents());

  // Initially, dirty bit is false.
  EXPECT_FALSE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
  // Simulate blocked subresource with activation enabled.
  test_web_contents_helper->NotifyOnBlockedSubresource(
      ActivationLevel::kEnabled);
  // Dirty bit should be true.
  EXPECT_TRUE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
  // Navigate somewhere else.
  WebContents* web_contents = RenderViewHostTestHarness::web_contents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents, GURL("http://google.test"));
  // Now dirty bit should be false again.
  EXPECT_FALSE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
}

TEST_F(
    FingerprintingProtectionNotifyOnBlockedSubresourceTest,
    SubresourceBlockedDirtyBit_SetOnBlockAndResetOnNavigation_ActivationDryRun) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.content_settings(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/false);

  auto* test_web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          RenderViewHostTestHarness::web_contents());

  // Initially, dirty bit is false.
  EXPECT_FALSE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
  // Simulate blocked subresource in dry run.
  test_web_contents_helper->NotifyOnBlockedSubresource(
      ActivationLevel::kDryRun);
  // Dirty bit should be true.
  EXPECT_TRUE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
  // Navigate somewhere else.
  WebContents* web_contents = RenderViewHostTestHarness::web_contents();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents, GURL("http://google.test"));
  // Now dirty bit should be false again.
  EXPECT_FALSE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
}

TEST_F(FingerprintingProtectionNotifyOnBlockedSubresourceTest,
       SubresourceBlockedDirtyBit_NotResetOnAbortedNavigation) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
      test_support_.content_settings(),
      test_support_.tracking_protection_settings(),
      /*dealer=*/nullptr,
      /*is_incognito=*/false);

  auto* test_web_contents_helper =
      FingerprintingProtectionWebContentsHelper::FromWebContents(
          RenderViewHostTestHarness::web_contents());

  // Initially, dirty bit is false.
  EXPECT_FALSE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
  // Simulate blocked subresource.
  test_web_contents_helper->NotifyOnBlockedSubresource(
      ActivationLevel::kEnabled);
  // Dirty bit should be true.
  EXPECT_TRUE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
  // Simulate aborted navigation.
  WebContents* web_contents = RenderViewHostTestHarness::web_contents();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://google.test"), web_contents);
  simulator->AbortCommit();
  // Dirty bit should not be reset - should still be true.
  EXPECT_TRUE(
      test_web_contents_helper->subresource_blocked_in_current_primary_page());
}

static constexpr std::string_view kGoogleUrl = "http://google.test/";
static constexpr std::string_view kYoutubeUrl = "http://youtube.test/";
static constexpr std::string_view kAppspotUrl = "http://appspot.test/";
static constexpr std::string_view kNonReloadedSiteUrl =
    "http://nonreloadedsite.test/";
static constexpr ukm::SourceId kGoogleUkmId = 1234;
static constexpr ukm::SourceId kYoutubeUkmId = 5678;
static constexpr ukm::SourceId kAppspotUkmId = 9999;
static constexpr ukm::SourceId kNonReloadedSiteUkmId = 4444;

class FakeRefreshMetricsManager : public RefreshMetricsManager {
 public:
  ukm::SourceId GetUkmSourceId(WebContents& web_contents) const override {
    return ukm_ids_.at(
        web_contents.GetVisibleURL().possibly_invalid_spec().c_str());
  }

 private:
  const base::flat_map<std::string_view, ukm::SourceId> ukm_ids_ = {
      {kGoogleUrl, kGoogleUkmId},
      {kYoutubeUrl, kYoutubeUkmId},
      {kAppspotUrl, kAppspotUkmId},
      {kNonReloadedSiteUrl, kNonReloadedSiteUkmId}};
};

// Inherits everything from FingerprintingProtectionWebContentsHelper, except
// for using a MockRefreshMetricsManager and implementing the factory functions
// as necessary.
class MockWCHForRefreshCountTests
    : public FingerprintingProtectionWebContentsHelper {
 public:
  static void CreateForWebContents(
      WebContents* web_contents,
      PrefService* pref_service,
      HostContentSettingsMap* content_settings,
      TrackingProtectionSettings* tracking_protection_settings,
      VerifiedRulesetDealer::Handle* dealer_handle,
      bool is_incognito) {
    // Do nothing if a FingerprintingProtectionWebContentsHelper already exists
    // for the current WebContents.
    if (FromWebContents(web_contents)) {
      return;
    }

    content::WebContentsUserData<MockWCHForRefreshCountTests>::
        CreateForWebContents(web_contents, pref_service, content_settings,
                             tracking_protection_settings, dealer_handle,
                             is_incognito);
  }

  static MockWCHForRefreshCountTests* FromWebContents(
      WebContents* web_contents) {
    return WebContentsUserData<MockWCHForRefreshCountTests>::FromWebContents(
        web_contents);
  }

  explicit MockWCHForRefreshCountTests(
      WebContents* web_contents,
      PrefService* pref_service,
      HostContentSettingsMap* content_settings,
      TrackingProtectionSettings* tracking_protection_settings,
      VerifiedRulesetDealer::Handle* dealer_handle,
      bool is_incognito)
      : FingerprintingProtectionWebContentsHelper(web_contents,
                                                  pref_service,
                                                  content_settings,
                                                  tracking_protection_settings,
                                                  dealer_handle,
                                                  is_incognito) {}

  RefreshMetricsManager& GetRefreshMetricsManager() override {
    return refresh_metrics_manager_;
  }

  FakeRefreshMetricsManager refresh_metrics_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
WEB_CONTENTS_USER_DATA_KEY_IMPL(MockWCHForRefreshCountTests);

class FingerprintingProtectionRefreshCountMetricsTest
    : public content::RenderViewHostTestHarness {
 public:
  FingerprintingProtectionRefreshCountMetricsTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    // Enable feature flag - necessary for creating FPWebContentsHelper.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kEnableFingerprintingProtectionFilter,
         features::kEnableFingerprintingProtectionFilterInIncognito},
        /*disabled_features=*/{});
  }

  void InitializeWebContentsHelper(bool is_incognito) {
    MockWCHForRefreshCountTests::CreateForWebContents(
        RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
        test_support_.content_settings(),
        test_support_.tracking_protection_settings(),
        /*dealer_handle=*/nullptr, is_incognito);
  }

  void TearDown() override { RenderViewHostTestHarness::TearDown(); }

  // Navigate to the given URL, then simulate subresource blocked and reload n
  // times.
  void NavigateBlockSubresourceAndReloadNTimes(std::string_view url,
                                               int n,
                                               bool block_subresource = true) {
    WebContents* web_contents = RenderViewHostTestHarness::web_contents();
    MockWCHForRefreshCountTests* web_contents_helper =
        MockWCHForRefreshCountTests::FromWebContents(web_contents);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                               GURL(url));
    for (int i = 0; i < n; i++) {
      if (block_subresource) {
        web_contents_helper->NotifyOnBlockedSubresource(
            ActivationLevel::kEnabled);
      }
      content::NavigationSimulator::Reload(web_contents);
    }
  }

  // Helper function to check expected UKM entries.
  void ExpectRefreshCountUkmsAre(
      base::flat_map<ukm::SourceId, int> expected_counts) {
    auto ukm_entries = ukm_recorder_.GetEntriesByName(
        ukm::builders::FingerprintingProtectionUsage::kEntryName);
    EXPECT_EQ(ukm_entries.size(),
              static_cast<unsigned long>(expected_counts.size()));
    for (const auto& entry : ukm_entries) {
      ukm::SourceId source_id = entry->source_id;
      ukm_recorder_.ExpectEntryMetric(
          entry,
          ukm::builders::FingerprintingProtectionUsage::kRefreshCountName,
          expected_counts[source_id]);
    }
  }

 protected:
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSupport test_support_;
};

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       OneRefreshOnOneSite_LogsRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 1, 1);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 1}});
}

TEST_F(
    FingerprintingProtectionRefreshCountMetricsTest,
    OneRefreshOnOneSite_NoBlockedSubresource_NoLoggedRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1, false);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectTotalCount(RefreshCountHistogramName, 0);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       MultipleRefreshesOnOneSite_LogsRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 3, 1);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 3}});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       MultipleNonContiguousRefreshesOnOneSite_LogsRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 3, 1);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 3}});
}

TEST_F(
    FingerprintingProtectionRefreshCountMetricsTest,
    MultipleNonContiguousRefreshesOnOneSite_NotAlwaysBlocked_LogsRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 0);
  // Don't block subresource on the last 2 -> only 1 effective refresh.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2, false);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 1}});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       OneRefreshEachOnMultipleSites_LogsRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 1);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 1, 3);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 3);

  // Expected UKMs
  ExpectRefreshCountUkmsAre(
      {{kGoogleUkmId, 1}, {kYoutubeUkmId, 1}, {kAppspotUkmId, 1}});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       MultipleRefreshesEachOnMultipleSites_LogsRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 2);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 4);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 3, 1);
  histograms.ExpectBucketCount(RefreshCountHistogramName, 2, 1);
  histograms.ExpectBucketCount(RefreshCountHistogramName, 4, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre(
      {{kGoogleUkmId, 3}, {kYoutubeUkmId, 2}, {kAppspotUkmId, 4}});
}

TEST_F(
    FingerprintingProtectionRefreshCountMetricsTest,
    MultipleNonContiguousRefreshesOnMultipleSites_LogsRefreshMetrics_Incognito) {
  InitializeWebContentsHelper(/*is_incognito=*/true);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 2);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 2);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 2, 1);
  histograms.ExpectBucketCount(RefreshCountHistogramName, 4, 2);

  // Expected UKMs
  ExpectRefreshCountUkmsAre(
      {{kGoogleUkmId, 2}, {kYoutubeUkmId, 4}, {kAppspotUkmId, 4}});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       OneRefreshOnOneSite_LogsRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 1, 1);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 1}});
}

TEST_F(
    FingerprintingProtectionRefreshCountMetricsTest,
    OneRefreshOnOneSite_NoBlockedSubresource_NoLoggedRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1, false);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectTotalCount(RefreshCountHistogramName, 0);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       MultipleRefreshesOnOneSite_LogsRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 3, 1);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 3}});
}

TEST_F(
    FingerprintingProtectionRefreshCountMetricsTest,
    MultipleNonContiguousRefreshesOnOneSite_LogsRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 3, 1);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 3}});
}

TEST_F(
    FingerprintingProtectionRefreshCountMetricsTest,
    MultipleNonContiguousRefreshesOnOneSite_NotAlwaysBlocked_LogsRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 0);
  // Don't block subresource on the last 2 -> only 1 effective refresh.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2, false);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectTotalCount(RefreshCountHistogramName, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre({{kGoogleUkmId, 1}});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       OneRefreshEachOnMultipleSites_LogsRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 1);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 1, 3);
  histograms.ExpectTotalCount(RefreshCountHistogramName, 3);

  // Expected UKMs
  ExpectRefreshCountUkmsAre(
      {{kGoogleUkmId, 1}, {kYoutubeUkmId, 1}, {kAppspotUkmId, 1}});
}

TEST_F(FingerprintingProtectionRefreshCountMetricsTest,
       MultipleRefreshesEachOnMultipleSites_LogsRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 2);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 4);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 3, 1);
  histograms.ExpectBucketCount(RefreshCountHistogramName, 2, 1);
  histograms.ExpectBucketCount(RefreshCountHistogramName, 4, 1);

  // Expected UKMs
  ExpectRefreshCountUkmsAre(
      {{kGoogleUkmId, 3}, {kYoutubeUkmId, 2}, {kAppspotUkmId, 4}});
}

TEST_F(
    FingerprintingProtectionRefreshCountMetricsTest,
    MultipleNonContiguousRefreshesOnMultipleSites_LogsRefreshMetrics_NonIncognito) {
  InitializeWebContentsHelper(/*is_incognito=*/false);

  base::HistogramTester histograms;

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 2);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 2);

  // Close WebContents, triggering log.
  RenderViewHostTestHarness::DeleteContents();

  // Expected UMAs
  histograms.ExpectBucketCount(RefreshCountHistogramName, 2, 1);
  histograms.ExpectBucketCount(RefreshCountHistogramName, 4, 2);

  // Expected UKMs
  ExpectRefreshCountUkmsAre(
      {{kGoogleUkmId, 2}, {kYoutubeUkmId, 4}, {kAppspotUkmId, 4}});
}

// Note: In these tests, when testing the latency UMA, we just check whether it
// is logged at all, as the measurement is done by a subresource filter library
// macro (UMA_HISTOGRAM_CUSTOM_MICRO_TIMES).
class FingerprintingProtectionRefreshCountExceptionTest
    : public content::RenderViewHostTestHarness {
 public:
  FingerprintingProtectionRefreshCountExceptionTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    // Enable feature param for adding exception in both regular and incognito.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kEnableFingerprintingProtectionFilter,
          {{features::kRefreshHeuristicExceptionThresholdParam, "3"},
           {"performance_measurement_rate", "1.0"}}},
         {features::kEnableFingerprintingProtectionFilterInIncognito,
          {{features::kRefreshHeuristicExceptionThresholdParam, "3"},
           {"performance_measurement_rate", "1.0"}}}},
        {});
  }

  void InitializeWebContentsHelper(bool is_incognito) {
    MockWCHForRefreshCountTests::CreateForWebContents(
        RenderViewHostTestHarness::web_contents(), test_support_.prefs(),
        test_support_.content_settings(),
        test_support_.tracking_protection_settings(),
        /*dealer_handle=*/nullptr, is_incognito);
  }

  void TearDown() override { RenderViewHostTestHarness::TearDown(); }

  // Navigate to the given URL, then simulate subresource blocked and reload n
  // times.
  void NavigateBlockSubresourceAndReloadNTimes(std::string_view url,
                                               int n,
                                               bool block_subresource = true) {
    WebContents* web_contents = RenderViewHostTestHarness::web_contents();
    MockWCHForRefreshCountTests* web_contents_helper =
        MockWCHForRefreshCountTests::FromWebContents(web_contents);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                               GURL(url));
    for (int i = 0; i < n; i++) {
      if (block_subresource) {
        web_contents_helper->NotifyOnBlockedSubresource(
            ActivationLevel::kEnabled);
      }
      content::NavigationSimulator::Reload(web_contents);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSupport test_support_;
};
TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NotEnoughRefreshesOnOneSite_NoException_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 0);

  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       RefreshesOnOneSite_HasException_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 1);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 1);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              1);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NonContiguousRefreshesOnOneSite_HasException_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  // No exception for the non-refreshed site.
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  // UMAs for one exception added.
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 1);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 1);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              1);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NotEnoughRefreshesOnMultipleSites_NoExceptions_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 1);

  // Not enough refreshes to add exception for any site.
  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       RefreshesOnMultipleSites_AllHaveExceptions_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 4);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 5);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_TRUE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_TRUE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  // UMAs for 3 exceptions added
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 3);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 3);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              3);
}

TEST_F(
    FingerprintingProtectionRefreshCountExceptionTest,
    NonContiguousRefreshesOfDifferentAmountsOnMultipleSites_SomeHaveExceptions_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  // Navigate between different sites and reload multiple times each, but only
  // Google and Appspot are refreshed enough to have an exception.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 0);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_TRUE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  EXPECT_FALSE(
      HasBreakageException(GURL(kNonReloadedSiteUrl), *test_support_.prefs()));
  // UMAs for 2 exceptions added
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 2);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 2);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              2);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       RefreshHeuristicParamDisabled_NoException_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  // Disable feature param for exception.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilterInIncognito,
      {{features::kRefreshHeuristicExceptionThresholdParam, "0"}});

  // Navigate and reload multiple times each.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 4);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 5);

  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NoBlockedSubresource_NoException_Incognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/true);

  // Navigate and reload multiple times each, but no blocked subresources.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3, false);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 4, false);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 5, false);

  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NotEnoughRefreshesOnOneSite_NoException_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 0);

  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       RefreshesOnOneSite_HasException_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 1);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 1);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              1);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NonContiguousRefreshesOnOneSite_HasException_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  // No exception for the non-refreshed site.
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  // UMAs for one exception added.
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 1);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 1);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              1);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NotEnoughRefreshesOnMultipleSites_NoExceptions_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 1);

  // Not enough refreshes to add exception for any site.
  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       RefreshesOnMultipleSites_AllHaveExceptions_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 4);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 5);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_TRUE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_TRUE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  // UMAs for three exceptions added.
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 3);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 3);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              3);
}

TEST_F(
    FingerprintingProtectionRefreshCountExceptionTest,
    NonContiguousRefreshesOfDifferentAmountsOnMultipleSites_SomeHaveExceptions_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  // Navigate between different sites and reload multiple times each, but only
  // Google and Appspot are refreshed enough to have an exception.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 1);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 2);
  NavigateBlockSubresourceAndReloadNTimes(kNonReloadedSiteUrl, 0);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 0);

  EXPECT_TRUE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_TRUE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  EXPECT_FALSE(
      HasBreakageException(GURL(kNonReloadedSiteUrl), *test_support_.prefs()));
  // UMAs for two exceptions added.
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 2);
  histograms.ExpectUniqueSample(AddRefreshCountExceptionHistogramName, 1, 2);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              2);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       RefreshHeuristicParamDisabled_NoException_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  // Disable feature param for exception.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{features::kRefreshHeuristicExceptionThresholdParam, "0"}});

  // Navigate and reload multiple times each.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 4);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 5);

  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

TEST_F(FingerprintingProtectionRefreshCountExceptionTest,
       NoBlockedSubresource_NoException_NonIncognito) {
  base::HistogramTester histograms;
  InitializeWebContentsHelper(/*is_incognito=*/false);

  // Navigate and reload multiple times each, but no blocked subresources.
  NavigateBlockSubresourceAndReloadNTimes(kGoogleUrl, 3, false);
  NavigateBlockSubresourceAndReloadNTimes(kYoutubeUrl, 4, false);
  NavigateBlockSubresourceAndReloadNTimes(kAppspotUrl, 5, false);

  EXPECT_FALSE(HasBreakageException(GURL(kGoogleUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kYoutubeUrl), *test_support_.prefs()));
  EXPECT_FALSE(HasBreakageException(GURL(kAppspotUrl), *test_support_.prefs()));
  histograms.ExpectTotalCount(AddRefreshCountExceptionHistogramName, 0);
  histograms.ExpectTotalCount(AddRefreshCountExceptionWallDurationHistogramName,
                              0);
}

}  // namespace
}  // namespace fingerprinting_protection_filter
