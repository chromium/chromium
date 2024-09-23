// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_PREVENT_CLOSE_TEST_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_PREVENT_CLOSE_TEST_BASE_H_

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/webapps/common/web_app_id.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif  // BUILDFLAG(IS_WIN)

class Browser;
class GURL;
class Profile;

class PreventCloseTestBase : public policy::PolicyTest {
 public:
  PreventCloseTestBase();
  PreventCloseTestBase(const PreventCloseTestBase&) = delete;
  PreventCloseTestBase& operator=(const PreventCloseTestBase&) = delete;
  ~PreventCloseTestBase() override;

  // policy::PolicyTest:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

  void SetPolicies(std::string_view web_app_settings,
                   std::string_view web_app_install_force_list);
  void SetPoliciesAndWaitUntilInstalled(
      const webapps::AppId& app_id,
      std::string_view web_app_settings,
      std::string_view web_app_install_force_list);
  void ClearWebAppSettings();
  void InstallPWA(const GURL& app_url, const webapps::AppId& app_id);
  Browser* LaunchPWA(const webapps::AppId& app_id, bool launch_in_window);
  base::Value ReturnPolicyValueFromJson(std::string_view policy);

  Profile* profile();

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kDesktopPWAsRunOnOsLogin};
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_PREVENT_CLOSE_TEST_BASE_H_
