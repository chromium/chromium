// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

class HistoryClustersSidePanelCoordinatorBrowserTest
    : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(HistoryClustersSidePanelCoordinatorBrowserTest,
                       ShowRespectsManagedPolicy) {
  // Verify that history clusters opens when configured by policy.
  HistoryClustersSidePanelCoordinator* const history_clusters_coordinator =
      browser()->GetFeatures().history_clusters_side_panel_coordinator();
  policy::PolicyMap policies;
  policies.Set(policy::key::kHistoryClustersVisible,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_TRUE(
      HistoryClustersSidePanelCoordinator::IsSupported(browser()->profile()));
  EXPECT_TRUE(history_clusters_coordinator->Show(std::string()));

  // Verify that history clusters does not show when disabled.
  policies.Set(policy::key::kHistoryClustersVisible,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_FALSE(
      HistoryClustersSidePanelCoordinator::IsSupported(browser()->profile()));
  EXPECT_FALSE(history_clusters_coordinator->Show(std::string()));
}
