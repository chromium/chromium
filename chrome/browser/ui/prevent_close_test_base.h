// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PREVENT_CLOSE_TEST_BASE_H_
#define CHROME_BROWSER_UI_PREVENT_CLOSE_TEST_BASE_H_

#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"

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

  void SetWebAppSettings(base::StringPiece config);
  void ClearWebAppSettings();
  void InstallPWA(const GURL& app_url, const web_app::AppId& app_id);
  Browser* LaunchPWA(const web_app::AppId& app_id, bool launch_in_window);
  base::Value ReturnPolicyValueFromJson(base::StringPiece policy);

  Profile* profile();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

#endif  // CHROME_BROWSER_UI_PREVENT_CLOSE_TEST_BASE_H_
