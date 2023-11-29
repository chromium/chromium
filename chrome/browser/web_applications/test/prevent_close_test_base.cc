// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/prevent_close_test_base.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

PreventCloseTestBase::PreventCloseTestBase() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kDesktopPWAsEnforceWebAppSettingsPolicy,
                            features::kDesktopPWAsPreventClose},
      /*disabled_features=*/{});
}

PreventCloseTestBase::~PreventCloseTestBase() = default;

void PreventCloseTestBase::SetUpInProcessBrowserTestFixture() {
  provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void PreventCloseTestBase::TearDownInProcessBrowserTestFixture() {
  ClearWebAppSettings();
  policy::PolicyTest::TearDownInProcessBrowserTestFixture();
}

void PreventCloseTestBase::SetWebAppSettings(base::StringPiece config) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kWebAppSettings,
            ReturnPolicyValueFromJson(config));
  provider_.UpdateChromePolicy(policies);
}

void PreventCloseTestBase::ClearWebAppSettings() {
  SetWebAppSettings(/*config=*/"[]");
}

void PreventCloseTestBase::InstallPWA(const GURL& app_url,
                                      const webapps::AppId& app_id) {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  webapps::AppId installed_app_id = web_app::test::InstallWebApp(
      browser()->profile(), std::move(web_app_info));
  EXPECT_EQ(app_id, installed_app_id);
}

Browser* PreventCloseTestBase::LaunchPWA(const webapps::AppId& app_id,
                                         bool launch_in_window) {
  return launch_in_window
             ? web_app::LaunchWebAppBrowserAndWait(
                   profile(), app_id, WindowOpenDisposition::NEW_WINDOW)
             : web_app::LaunchBrowserForWebAppInTab(profile(), app_id);
}

base::Value PreventCloseTestBase::ReturnPolicyValueFromJson(
    base::StringPiece policy) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      policy, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(result.has_value()) << result.error().message;
  DCHECK(result->is_list());
  return std::move(*result);
}

Profile* PreventCloseTestBase::profile() {
  return browser()->profile();
}
