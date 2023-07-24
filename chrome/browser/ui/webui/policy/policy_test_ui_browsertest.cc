// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#endif

using testing::_;
using testing::Return;

namespace {
class PolicyTestPageUITest
    : public PlatformBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
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

  Profile* GetProfile() {
#if BUILDFLAG(IS_ANDROID)
    return chrome_test_utils::GetProfile(this);
#else
    return browser()->profile();
#endif
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  bool IsPolicyTestPageEnabledByFeature() { return std::get<0>(GetParam()); }
  bool IsPolicyTestPageEnabledByPolicy() { return std::get<1>(GetParam()); }

  // Returns true if this profile is not managed.
  bool IsPolicyTestPageEnabledByManagedProfile() {
    return std::get<2>(GetParam());
  }

  int GetProfileManagement() {
    if (IsPolicyTestPageEnabledByManagedProfile()) {
      return policy::EnterpriseManagementAuthority::NONE;
    } else {
      return policy::EnterpriseManagementAuthority::CLOUD;
    }
  }

  bool GetExpectedValue() {
    return IsPolicyTestPageEnabledByFeature() &&
           IsPolicyTestPageEnabledByPolicy() &&
           IsPolicyTestPageEnabledByManagedProfile();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // Verifies that the policy test page at chrome://policy/test is visible if
  // expected is true, or not visible if expected is false.
  void VerifyTestPageVisibility(bool expected) {
    if (expected) {  // Test page should be visible
      // getElementById returns null if the element is not found and ExecJs
      // returns whether an error was raised, so use .children here and below as
      // calling .children on null raises an error.
      const std::string kJavaScript =
          "document.getElementById('top-buttons').children;";
      EXPECT_TRUE(content::ExecJs(web_contents(), kJavaScript));
    } else {  // Main policy page should be visible.
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
  // Enable or disable managed profile as needed.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(GetProfile()),
      GetProfileManagement());
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  VerifyTestPageVisibility(GetExpectedValue());
}

INSTANTIATE_TEST_SUITE_P(PolicyTestPageUITestInstance,
                         PolicyTestPageUITest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

namespace {
class PolicyTestHandlerTest : public PlatformBrowserTest {
 public:
  PolicyTestHandlerTest() {
    scoped_feature_list_.InitWithFeatureState(
        policy::features::kEnablePolicyTestPage, true);
  }
  PolicyTestHandlerTest(const PolicyTestHandlerTest&) = delete;
  PolicyTestHandlerTest& operator=(const PolicyTestHandlerTest&) = delete;

  ~PolicyTestHandlerTest() override = default;

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  std::unique_ptr<PolicyUIHandler> SetUpHandler() {
    auto handler = std::make_unique<PolicyUIHandler>();
    web_ui()->set_web_contents(web_contents());
    handler->set_web_ui_for_test(web_ui());
    handler->RegisterMessages();
    return handler;
  }

 protected:
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::TestWebUI web_ui_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest,
                       HandleSetAndRevertLocalTestPolicies) {
  std::unique_ptr<PolicyUIHandler> handler = SetUpHandler();
  const std::string jsonString =
      R"([
      {"level": 0,"scope": 0,"source": 0,
      "name": "AutofillAddressEnabled","value": false},
      {"level": 1,"scope": 1,"source": 2,
      "name": "CloudReportingEnabled","value": true}
      ])";

  base::Value::List list_args;

  list_args.Append("setLocalTestPolicies");
  list_args.Append(jsonString);

  web_ui()->HandleReceivedMessage("setLocalTestPolicies", list_args);

  base::RunLoop().RunUntilIdle();

  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  Profile* profile = chrome_test_utils::GetProfile(this);
  policy::PolicyService* policy_service =
      profile->GetProfilePolicyConnector()->policy_service();

  // Check correct policies applied
  const policy::PolicyMap* policy_map =
      &policy_service->GetPolicies(chrome_namespace);
  ASSERT_TRUE(policy_map);

  {
    const policy::PolicyMap::Entry* entry =
        policy_map->Get(policy::key::kAutofillAddressEnabled);
    ASSERT_TRUE(entry);
    const base::Value* value = entry->value(base::Value::Type::BOOLEAN);
    ASSERT_TRUE(value);
    EXPECT_EQ(base::Value(false), *value);
    EXPECT_EQ(entry->level, policy::POLICY_LEVEL_RECOMMENDED);
    EXPECT_EQ(entry->scope, policy::POLICY_SCOPE_USER);
    EXPECT_EQ(entry->source, policy::POLICY_SOURCE_ENTERPRISE_DEFAULT);
  }

  {
    const policy::PolicyMap::Entry* entry =
        policy_map->Get(policy::key::kCloudReportingEnabled);
    ASSERT_TRUE(entry);
    const base::Value* value = entry->value(base::Value::Type::BOOLEAN);
    ASSERT_TRUE(value);
    EXPECT_EQ(base::Value(true), *value);
    EXPECT_EQ(entry->level, policy::POLICY_LEVEL_MANDATORY);
    EXPECT_EQ(entry->scope, policy::POLICY_SCOPE_MACHINE);
    EXPECT_EQ(entry->source, policy::POLICY_SOURCE_CLOUD);
  }

  list_args.clear();
  list_args.Append("revertLocalTestPolicies");

  web_ui()->HandleReceivedMessage("revertLocalTestPolicies", list_args);

  base::RunLoop().RunUntilIdle();

  policy_map = &policy_service->GetPolicies(chrome_namespace);
  ASSERT_TRUE(policy_map);

  // Verify local test policies are no longer applied
  {
    const policy::PolicyMap::Entry* entry =
        policy_map->Get(policy::key::kAutofillAddressEnabled);
    EXPECT_FALSE(entry);
  }

  {
    const policy::PolicyMap::Entry* entry =
        policy_map->Get(policy::key::kCloudReportingEnabled);
    EXPECT_FALSE(entry);
  }

  handler.reset();
}

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest, FilterSensitivePolicies) {
  std::unique_ptr<PolicyUIHandler> handler = SetUpHandler();
  const std::string jsonString =
      R"([
      {"level": 0,"scope": 0,"source": 0,
      "name": "DefaultSearchProviderEnabled","value": false}
      ])";

  base::Value::List list_args;

  list_args.Append("setLocalTestPolicies");
  list_args.Append(jsonString);

  web_ui()->HandleReceivedMessage("setLocalTestPolicies", list_args);

  base::RunLoop().RunUntilIdle();

  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  Profile* profile = chrome_test_utils::GetProfile(this);
  policy::PolicyService* policy_service =
      profile->GetProfilePolicyConnector()->policy_service();

  const policy::PolicyMap* policy_map =
      &policy_service->GetPolicies(chrome_namespace);
  ASSERT_TRUE(policy_map);

  // Check sensitive policies not applied
  const policy::PolicyMap::Entry* entry =
      policy_map->Get(policy::key::kDefaultSearchProviderEnabled);
  EXPECT_FALSE(entry);

  handler.reset();
}
