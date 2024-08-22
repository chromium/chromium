// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string_view>

#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/local_test_policy_provider.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/policy_utils.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#endif

using testing::_;
using testing::Return;

namespace {
class PolicyTestPageVisibilityTest
    : public PlatformBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  PolicyTestPageVisibilityTest() = default;
  PolicyTestPageVisibilityTest(const PolicyTestPageVisibilityTest&) = delete;
  PolicyTestPageVisibilityTest& operator=(const PolicyTestPageVisibilityTest&) =
      delete;

  ~PolicyTestPageVisibilityTest() override = default;

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

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  bool IsPolicyTestPageEnabledByPolicy() { return std::get<0>(GetParam()); }

  // Returns true if this profile is not managed.
  bool IsPolicyTestPageEnabledByManagedProfile() {
    return std::get<1>(GetParam());
  }

  int GetProfileManagement() {
    if (IsPolicyTestPageEnabledByManagedProfile()) {
      return policy::EnterpriseManagementAuthority::NONE;
    } else {
      return policy::EnterpriseManagementAuthority::CLOUD;
    }
  }

  bool GetExpectedValue() {
    return IsPolicyTestPageEnabledByPolicy() &&
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
};

// Verify that the chrome://policy/test page is visible only when both the flag
// and policy are enabled, and invisible otherwise.
IN_PROC_BROWSER_TEST_P(PolicyTestPageVisibilityTest,
                       TestPageVisibleWhenEnabled) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  // Enable or disable managed profile as needed.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(GetProfile()),
      GetProfileManagement());
  const bool show_test_page = GetExpectedValue();
  EXPECT_EQ(show_test_page,
            content::NavigateToURL(web_contents(),
                                   GURL(chrome::kChromeUIPolicyTestURL)));
  if (show_test_page) {
    EXPECT_EQ(web_contents()->GetVisibleURL(),
              GURL(chrome::kChromeUIPolicyTestURL));
  } else {
    EXPECT_EQ(web_contents()->GetVisibleURL(),
              GURL(chrome::kChromeUIPolicyURL));
  }
  VerifyTestPageVisibility(GetExpectedValue());
}

INSTANTIATE_TEST_SUITE_P(PolicyTestPageUITestInstance,
                         PolicyTestPageVisibilityTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

class PolicyTestHandlerTest : public PlatformBrowserTest {
 public:
  PolicyTestHandlerTest() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
    if (policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                              chrome::GetChannel())) {
      SetUpRelaunchChromeOverrideForPRETests();
    }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  void SetUpRelaunchChromeOverrideForPRETests() {
    std::string_view test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    if (base::StartsWith(test_name, "PRE_")) {
      // Expect a browser relaunch late in browser shutdown.
      mock_relaunch_callback_ = std::make_unique<::testing::StrictMock<
          base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>>();
      EXPECT_CALL(*mock_relaunch_callback_, Run);
      relaunch_chrome_override_ =
          std::make_unique<upgrade_util::ScopedRelaunchChromeBrowserOverride>(
              mock_relaunch_callback_->Get());
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  /* Helper methods for executing JS strings. */
  content::EvalJsResult GetNumberOfRows() {
    const std::string getNumRowsJs =
        R"(
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelectorAll('policy-test-row')
            .length;
        )";
    return content::EvalJs(web_contents(), getNumRowsJs);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 protected:
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  content::TestWebUI web_ui_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<
      base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>
      mock_relaunch_callback_;
  std::unique_ptr<upgrade_util::ScopedRelaunchChromeBrowserOverride>
      relaunch_chrome_override_;
#endif
};

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest,
                       HandleSetLocalTestPoliciesNotSupported) {
  // Ensure chrome://policy/test not supported.
  policy::ScopedManagementServiceOverrideForTesting profile_management(
      policy::ManagementServiceFactory::GetForProfile(GetProfile()),
      policy::EnterpriseManagementAuthority::CLOUD);
  std::unique_ptr<PolicyUIHandler> handler = SetUpHandler();
  const std::string jsonString =
      R"([
      {"level": 0,"scope": 0,"source": 0, "namespace": "chrome",
       "name": "AutofillAddressEnabled","value": false},
      {"level": 1,"scope": 1,"source": 2, "namespace": "chrome",
       "name": "CloudReportingEnabled","value": true}
      ])";
  const std::string revertAppliedPoliciesButtonDisabledJs =
      R"(
        document
          .querySelector('#revert-applied-policies')
          .disabled;
      )";

  base::Value::List list_args;

  list_args.Append("setLocalTestPolicies");
  list_args.Append(jsonString);
  list_args.Append("{}");

  // Open chrome://policy
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(chrome::kChromeUIPolicyURL)));
  web_ui()->HandleReceivedMessage("setLocalTestPolicies", list_args);

  base::RunLoop().RunUntilIdle();

  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();

  // Check policies not applied
  const policy::PolicyMap* policy_map =
      &policy_service->GetPolicies(chrome_namespace);
  ASSERT_TRUE(policy_map);

  {
    const policy::PolicyMap::Entry* entry =
        policy_map->Get(policy::key::kAutofillAddressEnabled);
    ASSERT_FALSE(entry);
  }
}

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest,
                       HandleSetAndRevertLocalTestPolicies) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  std::unique_ptr<PolicyUIHandler> handler = SetUpHandler();
  const std::string jsonString =
      R"([
      {"level": 0,"scope": 0,"source": 0, "namespace": "chrome",
       "name": "AutofillAddressEnabled","value": false},
      {"level": 1,"scope": 1,"source": 2, "namespace": "chrome",
       "name": "CloudReportingEnabled","value": true}
      ])";
  const std::string revertAppliedPoliciesButtonDisabledJs =
      R"(
        document
          .querySelector('#revert-applied-policies')
          .disabled;
      )";

  base::Value::List list_args;

  list_args.Append("setLocalTestPolicies");
  list_args.Append(jsonString);
  list_args.Append("{}");

  // Open chrome://policy/test for the first time
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  EXPECT_EQ(GetNumberOfRows(), 1);
  EXPECT_EQ(
      content::EvalJs(web_contents(), revertAppliedPoliciesButtonDisabledJs),
      true);

  web_ui()->HandleReceivedMessage("setLocalTestPolicies", list_args);

  base::RunLoop().RunUntilIdle();

  // Navigate away and re-open chrome://policy/test for the first time
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIChromeURLsURL)));
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  EXPECT_EQ(GetNumberOfRows(), 2);
  EXPECT_EQ(
      content::EvalJs(web_contents(), revertAppliedPoliciesButtonDisabledJs),
      false);

  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();

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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GetProfile()->GetPrefs()->GetString(
                prefs::kUserCloudSigninPolicyResponseFromPolicyTestPage),
            "{}");
#endif

  list_args.clear();
  list_args.Append("revertLocalTestPolicies");

  web_ui()->HandleReceivedMessage("revertLocalTestPolicies", list_args);

  base::RunLoop().RunUntilIdle();

  // Navigate away and re-open chrome://policy/test for the first time
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIChromeURLsURL)));
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  EXPECT_EQ(GetNumberOfRows(), 1);
  EXPECT_EQ(
      content::EvalJs(web_contents(), revertAppliedPoliciesButtonDisabledJs),
      true);

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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GetProfile()->GetPrefs()->GetString(
                prefs::kUserCloudSigninPolicyResponseFromPolicyTestPage),
            "");
#endif

  handler.reset();
}

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest, FilterSensitivePolicies) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  std::unique_ptr<PolicyUIHandler> handler = SetUpHandler();
  const std::string jsonString =
      R"([
      {"level": 0,"scope": 0,"source": 0,
      "name": "DefaultSearchProviderEnabled","value": false}
      ])";

  base::Value::List list_args;

  list_args.Append("setLocalTestPolicies");
  list_args.Append(jsonString);
  list_args.Append("");

  web_ui()->HandleReceivedMessage("setLocalTestPolicies", list_args);

  base::RunLoop().RunUntilIdle();

  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();

  const policy::PolicyMap* policy_map =
      &policy_service->GetPolicies(chrome_namespace);
  ASSERT_TRUE(policy_map);

  // Check sensitive policies not applied
  const policy::PolicyMap::Entry* entry =
      policy_map->Get(policy::key::kDefaultSearchProviderEnabled);
  EXPECT_FALSE(entry);

  handler.reset();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
// TODO(b:293632195) Implement test on android and chromeos
// Test that test policies stored in the pref kLocalTestPoliciesForNextStartup
// are applied after restart.
IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest,
                       PRE_PRE_ApplyTestPoliciesAfterRestart) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  std::unique_ptr<PolicyUIHandler> handler = SetUpHandler();

  // Set the value of AccessCodeCastDeviceDuration to 100.
  const std::string jsonString =
      R"([
      {"level": 0,"scope": 0,"source": 0,
      "name": "AccessCodeCastDeviceDuration","value": 100}
      ])";

  base::Value::List list_args;

  list_args.Append("restartBrowser");
  list_args.Append(jsonString);

  // Restart the browser.
  web_ui()->HandleReceivedMessage("restartBrowser", list_args);

  handler.reset();
}

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest,
                       PRE_ApplyTestPoliciesAfterRestart) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();
  const policy::PolicyMap* policy_map =
      &policy_service->GetPolicies(chrome_namespace);

  ASSERT_TRUE(policy_map);

  // Check that the AccessCodeCastDeviceDuration policy is in the policy map and
  // its value is 100.
  const policy::PolicyMap::Entry* entry =
      policy_map->Get(policy::key::kAccessCodeCastDeviceDuration);
  EXPECT_TRUE(entry);
  EXPECT_EQ(entry->value(base::Value::Type::INTEGER)->GetInt(), 100);

  // Restart the browser.
  chrome::AttemptRestart();
}

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTest, ApplyTestPoliciesAfterRestart) {
  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();
  const policy::PolicyMap* policy_map =
      &policy_service->GetPolicies(chrome_namespace);

  ASSERT_TRUE(policy_map);

  // Check that the AccessCodeCastDeviceDuration policy is not in the policy
  // map.
  const policy::PolicyMap::Entry* entry =
      policy_map->Get(policy::key::kAccessCodeCastDeviceDuration);
  EXPECT_FALSE(entry);
}

class PolicyTestHandlerTestDisabledByPolicy : public PolicyTestHandlerTest {
 public:
  PolicyTestHandlerTestDisabledByPolicy() {
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap policy_map;
    base::Value::List policy_list;
    policy_list.Append(policy::key::kPolicyTestPageEnabled);
    policy_map.Set(policy::key::kEnableExperimentalPolicies,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_PLATFORM,
                   base::Value(std::move(policy_list)), nullptr);
    policy_map.Set(policy::key::kPolicyTestPageEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_PLATFORM, base::Value(false), nullptr);

    provider_.UpdateChromePolicy(policy_map);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
    // `PolicyTestHandlerTest` only sets up relaunch chrome override if policy
    // testing is enabled, but these tests require it unconditionally.
    if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                               chrome::GetChannel())) {
      SetUpRelaunchChromeOverrideForPRETests();
    }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  }

  PolicyTestHandlerTestDisabledByPolicy(
      const PolicyTestHandlerTestDisabledByPolicy&) = delete;
  PolicyTestHandlerTestDisabledByPolicy& operator=(
      const PolicyTestHandlerTestDisabledByPolicy&) = delete;

  ~PolicyTestHandlerTestDisabledByPolicy() override = default;

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTestDisabledByPolicy,
                       PRE_PRE_ApplyTestPoliciesAfterRestart) {
  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();
  const policy::PolicyMap& policy_map =
      policy_service->GetPolicies(chrome_namespace);

  const policy::PolicyMap::Entry* policy_test_page_enabled =
      policy_map.Get(policy::key::kPolicyTestPageEnabled);
  ASSERT_TRUE(policy_test_page_enabled);
  ASSERT_FALSE(
      policy_test_page_enabled->value(base::Value::Type::BOOLEAN)->GetBool());
  std::unique_ptr<PolicyUIHandler> handler = SetUpHandler();

  // Set the value of AccessCodeCastDeviceDuration to 100.
  const std::string jsonString =
      R"([
      {"level": 0,"scope": 0,"source": 0,
      "name": "AccessCodeCastDeviceDuration","value": 100}
      ])";

  base::Value::List list_args;

  list_args.Append("restartBrowser");
  list_args.Append(jsonString);

  // Restart the browser.
  web_ui()->HandleReceivedMessage("restartBrowser", list_args);

  handler.reset();
}

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTestDisabledByPolicy,
                       PRE_ApplyTestPoliciesAfterRestart) {
  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();
  const policy::PolicyMap& policy_map =
      policy_service->GetPolicies(chrome_namespace);

  const policy::PolicyMap::Entry* policy_test_page_enabled =
      policy_map.Get(policy::key::kPolicyTestPageEnabled);
  ASSERT_TRUE(policy_test_page_enabled);
  ASSERT_FALSE(
      policy_test_page_enabled->value(base::Value::Type::BOOLEAN)->GetBool());

  // Check that the AccessCodeCastDeviceDuration policy is not in the policy
  // map.
  const policy::PolicyMap::Entry* entry =
      policy_map.Get(policy::key::kAccessCodeCastDeviceDuration);
  EXPECT_FALSE(entry);

  // Restart the browser.
  chrome::AttemptRestart();
}

IN_PROC_BROWSER_TEST_F(PolicyTestHandlerTestDisabledByPolicy,
                       ApplyTestPoliciesAfterRestart) {
  const policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME,
                                                 std::string());
  policy::PolicyService* policy_service =
      GetProfile()->GetProfilePolicyConnector()->policy_service();
  const policy::PolicyMap& policy_map =
      policy_service->GetPolicies(chrome_namespace);

  const policy::PolicyMap::Entry* policy_test_page_enabled =
      policy_map.Get(policy::key::kPolicyTestPageEnabled);
  ASSERT_TRUE(policy_test_page_enabled);
  ASSERT_FALSE(
      policy_test_page_enabled->value(base::Value::Type::BOOLEAN)->GetBool());

  // Check that the AccessCodeCastDeviceDuration policy is not in the policy
  // map.
  const policy::PolicyMap::Entry* entry =
      policy_map.Get(policy::key::kAccessCodeCastDeviceDuration);
  EXPECT_FALSE(entry);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

namespace {

const char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kExtensionSchemaJson[] = R"({
  "type": "object",
  "properties": {
    "normal_boolean": {
      "type": "boolean"
    },
    "normal_number": {
      "type": "number"
    },
    "sensitive_number": {
      "type": "number",
      "sensitiveValue": true,
    }
  }
})";

class PolicyTestUITest : public PlatformBrowserTest {
 public:
  PolicyTestUITest() = default;
  PolicyTestUITest(const PolicyTestUITest&) = delete;
  PolicyTestUITest& operator=(const PolicyTestUITest&) = delete;

  ~PolicyTestUITest() override = default;

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    // Add a fake "extension" to the SchemaRegistry.
    auto* registry = GetProfile()->GetPolicySchemaRegistryService()->registry();
    const auto schema = policy::Schema::Parse(kExtensionSchemaJson);
    ASSERT_TRUE(schema.has_value()) << schema.error();
    registry->RegisterComponent(
        policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS, kExtensionId),
        *schema);

    // Enable kPolicyTestPageEnabled policy.
    policy::PolicyMap policy_map;
    base::Value::List policy_list;
    policy_list.Append(policy::key::kPolicyTestPageEnabled);
    policy_map.Set(policy::key::kEnableExperimentalPolicies,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_PLATFORM,
                   base::Value(std::move(policy_list)), nullptr);
    policy_map.Set(policy::key::kPolicyTestPageEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);

    provider_.UpdateChromePolicy(policy_map);

    // Set profile to unmanaged so chrome://policy/test is accessible.
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForProfile(GetProfile()),
        policy::EnterpriseManagementAuthority::NONE);
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

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  /* Helper methods for executing JS strings. */
  content::EvalJsResult GetNumberOfRows() {
    const std::string getNumRowsJs =
        R"(
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelectorAll('policy-test-row')
            .length;
        )";
    return content::EvalJs(web_contents(), getNumRowsJs);
  }

  bool ClickAddPolicy() {
    const std::string clickAddPolicyJs =
        R"(
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('#add-policy-btn')
            .click()
        )";
    return content::ExecJs(web_contents(), clickAddPolicyJs);
  }

 private:
  policy::Schema extension_schema_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestRowAddedWhenAddPolicyClicked) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  EXPECT_EQ(GetNumberOfRows(), 1);
  EXPECT_TRUE(ClickAddPolicy());
  EXPECT_EQ(GetNumberOfRows(), 2);
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestRowDeletedWhenRemoveClicked) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  EXPECT_EQ(GetNumberOfRows(), 1);
  const std::string clickRemoveJs =
      R"(
        document
          .querySelector('policy-test-table')
          .shadowRoot
          .querySelector('policy-test-row')
          .shadowRoot
          .querySelector('.remove-btn')
          .click();
      )";
  EXPECT_TRUE(content::ExecJs(web_contents(), clickRemoveJs));
  EXPECT_EQ(GetNumberOfRows(), 0);
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestPresetAutofill) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  const std::string getSelectedPresetId =
      R"(
        const presetDropdown =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.preset');
        presetDropdown
          .options[presetDropdown.selectedIndex]
          .id;
      )";
  EXPECT_EQ(content::EvalJs(web_contents(), getSelectedPresetId), "custom");
  const std::string getSourceValueJs =
      R"(
        const sourceDropdown =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.source');
        sourceDropdown
          .options[sourceDropdown.selectedIndex]
          .id;
      )";
  EXPECT_EQ(content::EvalJs(web_contents(), getSourceValueJs),
            "sourceEnterpriseDefault");
  const std::string changePresetToCbcmJs =
      R"(
        const presetDropdown =
          document
          .querySelector('policy-test-table')
          .shadowRoot
          .querySelector('policy-test-row')
          .shadowRoot
          .querySelector('.preset');
        presetDropdown.selectedIndex = 1;
        presetDropdown
          .dispatchEvent(new Event('change'));
      )";
  EXPECT_TRUE(content::ExecJs(web_contents(), changePresetToCbcmJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getSelectedPresetId), "cbcm");
  EXPECT_EQ(content::EvalJs(web_contents(), getSourceValueJs), "sourceCloud");
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, GetSchema) {
  base::Value schema = PolicyUI::GetSchema(GetProfile());
  ASSERT_NE(nullptr, schema.GetDict().FindDict(kExtensionId));
  // The extension's schema should exclude "sensitive_number", because it's
  // a sensitive policy.
  ASSERT_EQ(base::Value::Dict()
                .Set("normal_boolean", "boolean")
                .Set("normal_number", "number"),
            *schema.GetDict().FindDict(kExtensionId));
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestExtensionPoliciesIncluded) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }

  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));

  base::Value schema = PolicyUI::GetSchema(GetProfile());
  base::Value chrome_policy_names(base::Value::Type::LIST);
  for (const auto [key, _] : *schema.GetDict().FindDict("chrome")) {
    chrome_policy_names.GetList().Append(key);
  }
  base::Value extension_policy_names(base::Value::Type::LIST);
  for (const auto [key, _] : *schema.GetDict().FindDict(kExtensionId)) {
    extension_policy_names.GetList().Append(key);
  }

  // The namespace <select> should offer the Chrome policy domain, and the
  // extension's namespace.
  const std::string getNamespacesFromSelect =
      R"(
        Array.from(
            document
                .querySelector('policy-test-table')
                .shadowRoot
                .querySelector('policy-test-row')
                .shadowRoot
                .querySelector('.namespace')
                .options)
            .map(e => e.innerText)
      )";
  EXPECT_EQ(
      base::Value(base::Value::List().Append("Chrome").Append(kExtensionId)),
      content::EvalJs(web_contents(), getNamespacesFromSelect));

  // Get list of available policies from the <datalist>. They should initially
  // correspond to the Chrome policy domain.
  const std::string getPolicyNamesFromDatalist =
      R"(
        Array.from(
            document
                .querySelector('policy-test-table')
                .shadowRoot
                .querySelector('policy-test-row')
                .shadowRoot
                .getElementById('policy-name-list')
                .options)
            .map(e => e.innerText)
      )";
  content::EvalJsResult policy_names =
      content::EvalJs(web_contents(), getPolicyNamesFromDatalist);
  EXPECT_EQ(chrome_policy_names, policy_names);

  // Switch to the extension's namespace. This should change the <datalist>'s
  // policy names.
  std::string switchToExtensionNamespace =
      R"(
        const namespaceDropdown =
            document
                .querySelector('policy-test-table')
                .shadowRoot
                .querySelector('policy-test-row')
                .shadowRoot
                .querySelector('.namespace');
        namespaceDropdown.value = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa';
        namespaceDropdown.dispatchEvent(new CustomEvent('change'));
      )";
  std::ignore = content::EvalJs(web_contents(), switchToExtensionNamespace);
  content::EvalJsResult policy_names2 =
      content::EvalJs(web_contents(), getPolicyNamesFromDatalist);
  EXPECT_EQ(extension_policy_names, policy_names2);
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestSchemaChangeReflected) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }

  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));

  base::Value schema = PolicyUI::GetSchema(GetProfile());
  base::Value chrome_policy_names(base::Value::Type::LIST);
  for (const auto [key, _] : *schema.GetDict().FindDict("chrome")) {
    chrome_policy_names.GetList().Append(key);
  }
  base::Value extension_policy_names(base::Value::Type::LIST);
  for (const auto [key, _] : *schema.GetDict().FindDict(kExtensionId)) {
    extension_policy_names.GetList().Append(key);
  }

  // The namespace <select> should offer the Chrome policy domain, and the
  // extension's namespace.
  const std::string getNamespacesFromSelect =
      R"(
        Array.from(
            document
                .querySelector('policy-test-table')
                .shadowRoot
                .querySelector('policy-test-row')
                .shadowRoot
                .querySelector('.namespace')
                .options)
            .map(e => e.innerText)
      )";
  EXPECT_EQ(
      base::Value(base::Value::List().Append("Chrome").Append(kExtensionId)),
      content::EvalJs(web_contents(), getNamespacesFromSelect));

  // Delete the extension from the schema. It should disappear from the
  // namespace <select>.
  auto* registry = GetProfile()->GetPolicySchemaRegistryService()->registry();
  registry->UnregisterComponent(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS, kExtensionId));
  EXPECT_EQ(base::Value(base::Value::List().Append("Chrome")),
            content::EvalJs(web_contents(), getNamespacesFromSelect));
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestPolicyNameChangesInputType) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  const std::string getValueInputType =
      R"(
        document
          .querySelector('policy-test-table')
          .shadowRoot
          .querySelector('policy-test-row')
          .shadowRoot
          .querySelector('.value')
          .type
      )";
  EXPECT_EQ(content::EvalJs(web_contents(), getValueInputType), "text");

  // Select an integer-valued policy name and assert that the type has changed
  // to number.
  const std::string selectIntegerPolicyNameJs =
      R"(
        const nameDropdown =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.name');
        nameDropdown.value = 'CloudReportingUploadFrequency';
        nameDropdown.dispatchEvent(new Event('change'));
      )";
  EXPECT_TRUE(content::ExecJs(web_contents(), selectIntegerPolicyNameJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getValueInputType), "number");
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestErrorStateWhenNameNotSelected) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  const std::string getNameDropdownInErrorStateJs =
      R"(
        document.querySelector('policy-test-table')
          .shadowRoot
          .querySelector('policy-test-row')
          .shadowRoot
          .querySelector('.name')
          .classList
          .contains('error');
      )";
  EXPECT_EQ(content::EvalJs(web_contents(), getNameDropdownInErrorStateJs),
            false);
  const std::string clickApplyPoliciesJs =
      R"(document.querySelector('#apply-policies').click())";
  EXPECT_TRUE(content::ExecJs(web_contents(), clickApplyPoliciesJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getNameDropdownInErrorStateJs),
            true);

  // Select a policy name and assert that the name dropdown is not in the error
  // class, both before and after applying.
  const std::string selectPolicyNameJs =
      R"(
        const nameDropdown =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.name');
        nameDropdown.value = 'CloudReportingUploadFrequency';
        nameDropdown.dispatchEvent(new Event('focus'));
      )";
  EXPECT_TRUE(content::ExecJs(web_contents(), selectPolicyNameJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getNameDropdownInErrorStateJs),
            false);
  EXPECT_TRUE(content::ExecJs(web_contents(), clickApplyPoliciesJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getNameDropdownInErrorStateJs),
            false);
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestIncorrectValueTypeRaisesError) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  EXPECT_TRUE(ClickAddPolicy());
  const std::string getValueCellInErrorStateJs =
      R"(
        document
          .querySelector('policy-test-table')
          .shadowRoot
          .querySelector('policy-test-row')
          .shadowRoot
          .querySelector('.value')
          .classList
          .contains('error');
      )";
  EXPECT_EQ(content::EvalJs(web_contents(), getValueCellInErrorStateJs), false);

  // Select a policy name that expects an array value input, attempt to apply
  // policies with an integer value and assert that the value cell is added to
  // the error class.
  const std::string selectNameAndIncorrectValueAndApplyJs =
      R"(
        const nameDropdown =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.name');
        nameDropdown.value = 'CookiesAllowedForUrls';
        nameDropdown.dispatchEvent(new Event('change'));
        document
          .querySelector('policy-test-table')
          .shadowRoot
          .querySelector('policy-test-row')
          .shadowRoot
          .querySelector('.value')
          .value = '123';
        document.querySelector('#apply-policies').click();
      )";
  EXPECT_TRUE(
      content::ExecJs(web_contents(), selectNameAndIncorrectValueAndApplyJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getValueCellInErrorStateJs), true);

  // Try applying with an array value and assert that the value cell is not in
  // the error class before or after applying.
  const std::string focusOnValueCellJs =
      R"(
        const valueCell =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.value');
        valueCell.dispatchEvent(new Event('focus'));
      )";
  EXPECT_TRUE(content::ExecJs(web_contents(), focusOnValueCellJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getValueCellInErrorStateJs), false);
  const std::string applyWithValidValueJs =
      R"(
        document
          .querySelector('policy-test-table')
          .shadowRoot
          .querySelector('policy-test-row')
          .shadowRoot
          .querySelector('.value')
          .value = '[]';
        document.querySelector('#apply-policies').click();
      )";
  EXPECT_TRUE(content::ExecJs(web_contents(), applyWithValidValueJs));
  EXPECT_EQ(content::EvalJs(web_contents(), getValueCellInErrorStateJs), false);
}

IN_PROC_BROWSER_TEST_F(PolicyTestUITest, TestClearPoliciesButton) {
  if (!policy::utils::IsPolicyTestingEnabled(/*pref_service=*/nullptr,
                                             chrome::GetChannel())) {
    GTEST_SKIP() << "chrome://policy/test not allowed on this build.";
  }
  ASSERT_TRUE(content::NavigateToURL(web_contents(),
                                     GURL(chrome::kChromeUIPolicyTestURL)));
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(ClickAddPolicy());
  }
  EXPECT_EQ(GetNumberOfRows(), 5);
  const std::string selectPolicyNameJs =
      R"(
        const nameDropdown =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.name');
        nameDropdown.value = 'CloudReportingUploadFrequency';
      )";
  EXPECT_TRUE(content::ExecJs(web_contents(), selectPolicyNameJs));
  const std::string getSelectedPolicyNameJs =
      R"(
        const nameDropdown =
          document
            .querySelector('policy-test-table')
            .shadowRoot
            .querySelector('policy-test-row')
            .shadowRoot
            .querySelector('.name');
        nameDropdown.value;
      )";
  EXPECT_EQ(content::EvalJs(web_contents(), getSelectedPolicyNameJs),
            "CloudReportingUploadFrequency");
  const std::string clickClearJs =
      R"(document.querySelector('#clear-policies').click())";
  EXPECT_TRUE(content::ExecJs(web_contents(), clickClearJs));
  EXPECT_EQ(GetNumberOfRows(), 1);
  EXPECT_EQ(content::EvalJs(web_contents(), getSelectedPolicyNameJs), "");
}
}  // namespace
