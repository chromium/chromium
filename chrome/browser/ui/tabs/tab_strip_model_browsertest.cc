// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kShouldPreventClose = true;
#else
constexpr bool kShouldPreventClose = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class TabStripModelPreventCloseTest : public policy::PolicyTest {
 public:
  TabStripModelPreventCloseTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDesktopPWAsEnforceWebAppSettingsPolicy,
                              features::kDesktopPWAsPreventClose},
        /*disabled_features=*/{});
  }
  TabStripModelPreventCloseTest(const TabStripModelPreventCloseTest&) = delete;
  TabStripModelPreventCloseTest& operator=(
      const TabStripModelPreventCloseTest&) = delete;
  ~TabStripModelPreventCloseTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    ClearWebAppSettings();
    policy::PolicyTest::TearDownInProcessBrowserTestFixture();
  }

  void SetWebAppSettings(base::StringPiece config) {
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kWebAppSettings,
              ReturnPolicyValueFromJson(config));
    provider_.UpdateChromePolicy(policies);
  }

  void ClearWebAppSettings() { SetWebAppSettings(/*config=*/"[]"); }

  void InstallPWA(const GURL& app_url, const web_app::AppId& app_id) {
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url.GetWithoutFilename();
    web_app::AppId installed_app_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    EXPECT_EQ(app_id, installed_app_id);
  }

  Browser* LaunchPWA(const web_app::AppId& app_id, bool launch_in_window) {
    return launch_in_window
               ? web_app::LaunchWebAppBrowserAndWait(
                     profile(), app_id, WindowOpenDisposition::NEW_WINDOW)
               : web_app::LaunchBrowserForWebAppInTab(profile(), app_id);
  }

  base::Value ReturnPolicyValueFromJson(base::StringPiece policy) {
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        policy, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    DCHECK(result.has_value()) << result.error().message;
    DCHECK(result->is_list());
    return std::move(*result);
  }

  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(TabStripModelPreventCloseTest,
                       PreventCloseEnforedByPolicy) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetWebAppSettings(kPreventCloseEnabledForCalculator);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(!kShouldPreventClose, tab_strip_model->IsTabClosable(
                                      tab_strip_model->GetActiveWebContents()));

  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(kShouldPreventClose ? 1 : 0, tab_strip_model->count());

  if (kShouldPreventClose) {
    ClearWebAppSettings();
    EXPECT_TRUE(tab_strip_model->IsTabClosable(
        tab_strip_model->GetActiveWebContents()));

    tab_strip_model->CloseAllTabs();
    EXPECT_EQ(0, tab_strip_model->count());
  }
}

IN_PROC_BROWSER_TEST_F(TabStripModelPreventCloseTest,
                       PreventCloseEnforedByPolicyTabbedAppShallBeClosable) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetWebAppSettings(kPreventCloseEnabledForCalculator);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/false);
  ASSERT_TRUE(browser);

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  EXPECT_NE(0, tab_strip_model->count());
  EXPECT_TRUE(
      tab_strip_model->IsTabClosable(tab_strip_model->GetActiveWebContents()));

  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(0, tab_strip_model->count());
}
