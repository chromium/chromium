// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui.h"

#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"

class ManagementUITest : public InProcessBrowserTest {
 public:
  ManagementUITest() = default;

  ManagementUITest(const ManagementUITest&) = delete;
  ManagementUITest& operator=(const ManagementUITest&) = delete;

  ~ManagementUITest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    provider_.SetupPolicyServiceForPolicyUpdates(policy_service());
  }

  void TearDownOnMainThread() override {
    provider_.SetupPolicyServiceForPolicyUpdates(nullptr);
  }

  void VerifyTexts(
      base::Value* actual_values,
      const std::map<std::string, std::u16string>& expected_values) {
    base::Value::Dict& values_as_dict = actual_values->GetDict();
    for (const auto& val : expected_values) {
      const std::string* actual_value = values_as_dict.FindString(val.first);
      ASSERT_TRUE(actual_value);
      EXPECT_EQ(base::UTF8ToUTF16(*actual_value), val.second);
    }
  }
  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }

  policy::ProfilePolicyConnector* profile_policy_connector() {
    return browser()->profile()->GetProfilePolicyConnector();
  }

  policy::PolicyService* policy_service() {
    return browser()->profile()->GetProfilePolicyConnector()->policy_service();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(ManagementUITest, ManagementStateChange) {
  // Ensure the device is not considered managed.
  policy::ScopedManagementServiceOverrideForTesting plattform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);
  profile_policy_connector()->OverrideIsManagedForTesting(false);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://management")));

  // The browser is not managed.
  const std::string javascript =
      "window.ManagementBrowserProxyImpl.getInstance()"
      "  .getContextualManagedData()"
      "  .then(managed_result => "
      "    JSON.stringify(managed_result));";

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string unmanaged_json =
      content::EvalJs(contents, javascript).ExtractString();

  std::optional<base::Value> unmanaged_value_ptr =
      base::JSONReader::Read(unmanaged_json);
  std::map<std::string, std::u16string> expected_unmanaged_values{
      {"browserManagementNotice",
       l10n_util::GetStringFUTF16(
           IDS_MANAGEMENT_NOT_MANAGED_NOTICE, chrome::kManagedUiLearnMoreUrl,
           base::EscapeForHTML(l10n_util::GetStringUTF16(
               IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT)))},
      {"extensionReportingSubtitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED)},
      {"pageSubtitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE)},
      {"managedWebsitesSubtitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION)},
  };

  VerifyTexts(&*unmanaged_value_ptr, expected_unmanaged_values);

  // The browser is managed.
  profile_policy_connector()->OverrideIsManagedForTesting(true);

  policy::PolicyMap policy_map;
  policy_map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                 base::Value("hello world"), nullptr);
  provider()->UpdateExtensionPolicy(policy_map,
                                    kOnPremReportingExtensionBetaId);

  contents = browser()->tab_strip_model()->GetActiveWebContents();
  std::string managed_json =
      content::EvalJs(contents, javascript).ExtractString();

  std::optional<base::Value> managed_value_ptr =
      base::JSONReader::Read(managed_json);
  std::map<std::string, std::u16string> expected_managed_values{
      {"browserManagementNotice",
       l10n_util::GetStringFUTF16(
           IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
           base::EscapeForHTML(l10n_util::GetStringUTF16(
               IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT)))},
      {"extensionReportingSubtitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED)},
      {"pageSubtitle", l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE)},
      {"managedWebsitesSubtitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION)}};

  VerifyTexts(&*managed_value_ptr, expected_managed_values);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
