// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/management_ui.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"

class ManagementUITest : public InProcessBrowserTest {
 public:
  ManagementUITest() = default;
  ~ManagementUITest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void VerifyTexts(base::Value* actual_values,
                   std::map<std::string, base::string16>& expected_values) {
    base::DictionaryValue* values_as_dict = NULL;
    actual_values->GetAsDictionary(&values_as_dict);
    for (const auto& val : expected_values) {
      base::string16 actual_value;
      values_as_dict->GetString(val.first, &actual_value);
      ASSERT_EQ(actual_value, val.second);
    }
  }
  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }

  policy::ProfilePolicyConnector* profile_policy_connector() {
    return browser()->profile()->GetProfilePolicyConnector();
  }

 private:
  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(ManagementUITest);
};

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ManagementUITest, ManagementStateChange) {
  profile_policy_connector()->OverrideIsManagedForTesting(false);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://management"));

  // The browser is not managed.
  const std::string javascript =
      "window.ManagementBrowserProxyImpl.getInstance()"
      "  .getContextualManagedData()"
      "  .then(managed_result => "
      "    domAutomationController.send(JSON.stringify(managed_result)));";

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string unmanaged_json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents, javascript,
                                                     &unmanaged_json));

  std::unique_ptr<base::Value> unmanaged_value_ptr =
      base::JSONReader::ReadDeprecated(unmanaged_json);
  std::map<std::string, base::string16> expected_unmanaged_values{
      {"browserManagementNotice",
       l10n_util::GetStringFUTF16(
           IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
           base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl))},
      {"extensionReportingTitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED)},
      {"pageSubtitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE)},
  };

  VerifyTexts(unmanaged_value_ptr.get(), expected_unmanaged_values);

  // The browser is managed.
  profile_policy_connector()->OverrideIsManagedForTesting(true);

  policy::PolicyMap policy_map;
  policy_map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>("hello world"), nullptr);
  provider()->UpdateExtensionPolicy(policy_map,
                                    kOnPremReportingExtensionBetaId);

  contents = browser()->tab_strip_model()->GetActiveWebContents();
  std::string managed_json;

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents, javascript,
                                                     &managed_json));

  std::unique_ptr<base::Value> managed_value_ptr =
      base::JSONReader::ReadDeprecated(managed_json);
  std::map<std::string, base::string16> expected_managed_values{
      {"browserManagementNotice",
       l10n_util::GetStringFUTF16(
           IDS_MANAGEMENT_BROWSER_NOTICE,
           base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl))},
      {"extensionReportingTitle",
       l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED)},
      {"pageSubtitle", l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE)},
  };

  VerifyTexts(managed_value_ptr.get(), expected_managed_values);
}
#endif  // !defined(OS_CHROMEOS)
