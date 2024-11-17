// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/prevent_close_test_base.h"

#include <string_view>

#include "ash/constants/web_app_id_constants.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

PreventCloseTestBase::PreventCloseTestBase() = default;

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

void PreventCloseTestBase::SetPolicies(
    std::string_view web_app_settings,
    std::string_view web_app_install_force_list) {
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kWebAppSettings,
            ReturnPolicyValueFromJson(web_app_settings));
  SetPolicy(&policies, policy::key::kWebAppInstallForceList,
            ReturnPolicyValueFromJson(web_app_install_force_list));
  provider_.UpdateChromePolicy(policies);
}

void PreventCloseTestBase::SetPoliciesAndWaitUntilInstalled(
    const webapps::AppId& app_id,
    std::string_view web_app_settings,
    std::string_view web_app_install_force_list) {
  web_app::WebAppTestInstallObserver observer(browser()->profile());
  observer.BeginListening({app_id});

  SetPolicies(web_app_settings, web_app_install_force_list);

  const webapps::AppId installed_app_id = observer.Wait();
  ASSERT_EQ(installed_app_id, app_id);
}

void PreventCloseTestBase::ClearWebAppSettings() {
  SetPolicies(/*web_app_settings=*/"[]", /*web_app_install_force_list=*/"[]");
}

void PreventCloseTestBase::InstallPWA(const GURL& app_url,
                                      const webapps::AppId& app_id) {
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
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
    std::string_view policy) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      policy, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(result.has_value()) << result.error().message;
  DCHECK(result->is_list());
  return std::move(*result);
}

Profile* PreventCloseTestBase::profile() {
  return browser()->profile();
}
