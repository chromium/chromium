// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace {
class PolicyTestPageUITest
    : public PlatformBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  PolicyTestPageUITest() {
    // Enable or disable feature as needed
    scoped_feature_list_.InitWithFeatureState(
        policy::features::kEnablePolicyTestPage,
        IsPolicyTestPageEnabledByFeature());
  }
  PolicyTestPageUITest(const PolicyTestPageUITest&) = delete;
  PolicyTestPageUITest& operator=(const PolicyTestPageUITest&) = delete;

  ~PolicyTestPageUITest() override = default;

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    // Enable or disable policy as needed
    policy::PolicyMap policy_map;
    base::Value::List policy_list;
    policy_list.Append(policy::key::kPolicyTestPageEnabled);
    policy_map.Set(policy::key::kEnableExperimentalPolicies,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_PLATFORM,
                   base::Value(std::move(policy_list)), nullptr);
    policy_map.Set(policy::key::kPolicyTestPageEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_PLATFORM,
                   base::Value(IsPolicyTestPageEnabledByPolicy()), nullptr);

    provider_.UpdateChromePolicy(policy_map);
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    policy::PushProfilePolicyConnectorProviderForTesting(&provider_);
  }

  void UpdateProviderPolicyForNamespace(
      const policy::PolicyNamespace& policy_namespace,
      const policy::PolicyMap& policy) {
    policy::PolicyBundle bundle;
    bundle.Get(policy_namespace) = policy.Clone();
    provider_.UpdatePolicy(std::move(bundle));
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  bool IsPolicyTestPageEnabledByFeature() { return std::get<0>(GetParam()); }
  bool IsPolicyTestPageEnabledByPolicy() { return std::get<1>(GetParam()); }
  bool GetExpectedValue() {
    return IsPolicyTestPageEnabledByFeature() &&
           IsPolicyTestPageEnabledByPolicy();
  }
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // Verifies that the policy test page at chrome://policy/test is visible if
  // expected is true, or not visible if expected is false
  void VerifyTestPageVisibility(bool expected) {
    if (expected) {  // Test page should be visible
      // getElementById returns null if the element is not found and ExecJs
      // returns whether an error was raised, so use .children here and below as
      // calling .children on null raises an error.
      const std::string kJavaScript =
          "document.getElementById('top-buttons').children;";
      EXPECT_TRUE(content::ExecJs(web_contents(), kJavaScript));
    } else {  // Main policy page should be visible
      const std::string kJavaScript =
          "document.getElementById('topbar').children;";
      EXPECT_TRUE(content::ExecJs(web_contents(), kJavaScript));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace

// Verify that the chrome://policy/test page is visible only when both the flag
// and policy are enabled, and invisible otherwise.
IN_PROC_BROWSER_TEST_P(PolicyTestPageUITest, TestPageVisibleWhenEnabled) {
  // Navigate to test page
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  VerifyTestPageVisibility(GetExpectedValue());
}

INSTANTIATE_TEST_SUITE_P(PolicyTestPageUITestInstance,
                         PolicyTestPageUITest,
                         testing::Combine(testing::Bool(), testing::Bool()));
