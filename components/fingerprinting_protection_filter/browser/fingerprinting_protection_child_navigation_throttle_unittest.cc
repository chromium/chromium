// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_child_navigation_throttle.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {
namespace {

using ::subresource_filter::ChildFrameNavigationFilteringThrottleTestHarness;
using ::subresource_filter::SimulateCommitAndGetResult;
using ::subresource_filter::SimulateRedirectAndGetResult;
using ::subresource_filter::SimulateStartAndGetResult;

const char kDisallowedConsoleMessageFormat[] = "Placeholder: %s";

const char kFilterDelayDisallowed[] =
    "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay.Disallowed";
const char kFilterDelayWouldDisallow[] =
    "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay."
    "WouldDisallow";
const char kFilterDelayAllowed[] =
    "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay.Allowed";
const char kFilterDelayAliasDisallowed[] =
    "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay.NameAlias."
    "Disallowed";
const char kFilterDelayAliasWouldDisallow[] =
    "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay.NameAlias."
    "WouldDisallow";
const char kFilterDelayAliasChecked[] =
    "FingerprintingProtection.DocumentLoad.SubframeFilteringDelay.NameAlias."
    "Checked";

class FingerprintingProtectionChildNavigationThrottleTest
    : public ChildFrameNavigationFilteringThrottleTestHarness {
 public:
  FingerprintingProtectionChildNavigationThrottleTest() = default;

  FingerprintingProtectionChildNavigationThrottleTest(
      const FingerprintingProtectionChildNavigationThrottleTest&) = delete;
  FingerprintingProtectionChildNavigationThrottleTest& operator=(
      const FingerprintingProtectionChildNavigationThrottleTest&) = delete;

  ~FingerprintingProtectionChildNavigationThrottleTest() override = default;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_FALSE(navigation_handle->IsInMainFrame());
    // The |parent_filter_| is the parent frame's filter. Do not register a
    // throttle if the parent is not activated with a valid filter.
    if (parent_filter_) {
      auto throttle =
          std::make_unique<FingerprintingProtectionChildNavigationThrottle>(
              navigation_handle, parent_filter_.get(),
              base::BindRepeating([](const GURL& filtered_url) {
                // TODO(https://crbug.com/40280666): Implement new console
                // message.
                return base::StringPrintf(
                    kDisallowedConsoleMessageFormat,
                    filtered_url.possibly_invalid_spec().c_str());
              }));
      ASSERT_EQ("FingerprintingProtectionChildNavigationThrottle",
                std::string(throttle->GetNameForLogging()));
      navigation_handle->RegisterThrottleForTesting(std::move(throttle));
    }
  }
};

TEST_F(FingerprintingProtectionChildNavigationThrottleTest, DelayMetrics) {
  base::HistogramTester histogram_tester;
  ChildFrameNavigationFilteringThrottleTestHarness::
      InitializeDocumentSubresourceFilter(GURL("https://example.test"));
  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(
          GURL("https://example.test/allowed.html"), main_rfh());
  navigation_simulator()->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://example.test/disallowed.html")));

  navigation_simulator()->CommitErrorPage();

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 0);

  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(
          GURL("https://example.test/allowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 1);

  histogram_tester.ExpectTotalCount(kFilterDelayAliasChecked, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasDisallowed, 0);
}

TEST_F(FingerprintingProtectionChildNavigationThrottleTest,
       DelayMetricsDryRun) {
  base::HistogramTester histogram_tester;
  ChildFrameNavigationFilteringThrottleTestHarness::
      InitializeDocumentSubresourceFilter(
          GURL("https://example.test"),
          subresource_filter::mojom::ActivationLevel::kDryRun);
  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(
          GURL("https://example.test/allowed.html"), main_rfh());
  navigation_simulator()->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateRedirectAndGetResult(
                navigation_simulator(),
                GURL("https://example.test/disallowed.html")));
  navigation_simulator()->Commit();

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 0);

  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(
          GURL("https://example.test/allowed.html"), main_rfh());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 1);

  histogram_tester.ExpectTotalCount(kFilterDelayAliasChecked, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasDisallowed, 0);
}

class FingerprintingProtectionChildNavigationThrottleAliasTest
    : public FingerprintingProtectionChildNavigationThrottleTest {
 public:
  FingerprintingProtectionChildNavigationThrottleAliasTest() {
    feature_list_.InitAndEnableFeature(
        fingerprinting_protection_filter::features::
            kUseCnameAliasesForFingerprintingProtectionFilter);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FingerprintingProtectionChildNavigationThrottleAliasTest,
       FilterOnWillProcessResponse) {
  base::HistogramTester histogram_tester;
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"), {"disallowedcname.com"},
      subresource_filter::mojom::ActivationLevel::kEnabled);

  const GURL url = GURL("https://example.test/allowed.html");
  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(url, main_rfh());

  std::vector<std::string> dns_aliases({"disallowedcname.com"});
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 0);

  histogram_tester.ExpectTotalCount(kFilterDelayAliasChecked, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasDisallowed, 1);
}

TEST_F(FingerprintingProtectionChildNavigationThrottleAliasTest,
       DryRunOnWillProcessResponse) {
  base::HistogramTester histogram_tester;
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"),
      {"cnamed.com", "alias-me.com", "i.am.alias"},
      subresource_filter::mojom::ActivationLevel::kDryRun);

  const GURL url = GURL("https://example.test/allowed.html");
  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(url, main_rfh());

  std::vector<std::string> dns_aliases(
      {"cnamed.com", "def.not.alias.com", "alias.me.com"});
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayAliasChecked, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasWouldDisallow, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasDisallowed, 0);

  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(
          GURL("https://example.test/allowed.html"), main_rfh());
  std::vector<std::string> allowed_dns_aliases({"allowed.com", "allow-me.com"});
  SetResponseDnsAliasesForNavigation(std::move(allowed_dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 0);

  histogram_tester.ExpectTotalCount(kFilterDelayAliasChecked, 2);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasWouldDisallow, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasDisallowed, 0);
}

TEST_F(FingerprintingProtectionChildNavigationThrottleAliasTest,
       AllowedDnsAliasesShouldNotFilter) {
  base::HistogramTester histogram_tester;
  InitializeDocumentSubresourceFilterWithSubstringRules(
      GURL("https://example.test"),
      {"cnamed.com", "alias-me.com", "i.am.alias"},
      subresource_filter::mojom::ActivationLevel::kEnabled);

  const GURL url = GURL("https://example.test/allowed.html");
  ChildFrameNavigationFilteringThrottleTestHarness::
      CreateTestSubframeAndInitNavigation(url, main_rfh());

  std::vector<std::string> dns_aliases(
      {"disallowed.com", "dis-A-llowed.com", "allowed-to-dis.com"});
  SetResponseDnsAliasesForNavigation(std::move(dns_aliases));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateStartAndGetResult(navigation_simulator()));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            SimulateCommitAndGetResult(navigation_simulator()));

  histogram_tester.ExpectTotalCount(kFilterDelayDisallowed, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAllowed, 1);

  histogram_tester.ExpectTotalCount(kFilterDelayAliasChecked, 1);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasWouldDisallow, 0);
  histogram_tester.ExpectTotalCount(kFilterDelayAliasDisallowed, 0);
}
}  // namespace
}  // namespace fingerprinting_protection_filter
