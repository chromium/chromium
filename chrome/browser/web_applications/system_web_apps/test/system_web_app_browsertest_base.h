// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_SYSTEM_WEB_APP_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_SYSTEM_WEB_APP_BROWSERTEST_BASE_H_

#include <memory>

#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/in_process_browser_test.h"

class KeyedService;

namespace apps {
struct AppLaunchParams;
}

namespace content {
class WebContents;
}

namespace web_app {

enum class SystemAppType;

// Clients should use SystemWebAppManagerBrowserTest, so test can be run with
// both the new web apps provider and the legacy bookmark apps provider.
class SystemWebAppBrowserTestBase : public InProcessBrowserTest {
 public:
  // Performs common initialization for testing SystemWebAppManager features.
  // If true, |install_mock| installs a WebUIController that serves a mock
  // System PWA, and ensures the WebAppProvider associated with the startup
  // profile is a TestWebAppProviderCreator.
  explicit SystemWebAppBrowserTestBase(bool install_mock = true);
  SystemWebAppBrowserTestBase(const SystemWebAppBrowserTestBase&) = delete;
  SystemWebAppBrowserTestBase& operator=(const SystemWebAppBrowserTestBase&) =
      delete;

  ~SystemWebAppBrowserTestBase() override;

  // Returns the SystemWebAppManager for browser()->profile(). For incognito
  // profiles, this will be the SystemWebAppManager of the original profile.
  // Returns TestSystemWebAppManager if initialized with |install_mock| true.
  SystemWebAppManager& GetManager();

  // Returns SystemAppType of mocked app, only valid if |install_mock| is true.
  SystemAppType GetMockAppType();

  // Returns the start URL based on the given |params|.
  GURL GetStartUrl(const apps::AppLaunchParams& params);

  // Returns the default start url.
  GURL GetStartUrl();

  void WaitForTestSystemAppInstall();

  // Creates a default AppLaunchParams for |system_app_type|. Launches a window.
  // Uses kSourceTest as the AppLaunchSource.
  apps::AppLaunchParams LaunchParamsForApp(SystemAppType system_app_type);

  // Launch the given System App from |params|, and wait for the application to
  // finish loading. If |browser| is not nullptr, it will store the Browser*
  // that hosts the launched application.
  content::WebContents* LaunchApp(apps::AppLaunchParams&& params,
                                  Browser** browser = nullptr);

  // Launch the given System App |type| with default AppLaunchParams, and wait
  // for the application to finish loading. If |browser| is not nullptr, it will
  // store the Browser* that hosts the launched application.
  content::WebContents* LaunchApp(SystemAppType type,
                                  Browser** browser = nullptr);

  // Launch the given System App from |params|, without waiting for the
  // application to finish loading. If |browser| is not nullptr, it will store
  // the Browser* that hosts the launched application.
  content::WebContents* LaunchAppWithoutWaiting(apps::AppLaunchParams&& params,
                                                Browser** browser = nullptr);

  // Launch the given System App |type| with default AppLaunchParams, without
  // waiting for the application to finish loading. If |browser| is not nullptr,
  // it will store the Browser* that hosts the launched application.
  content::WebContents* LaunchAppWithoutWaiting(SystemAppType type,
                                                Browser** browser = nullptr);

 protected:
  std::unique_ptr<TestSystemWebAppInstallation> maybe_installation_;

 private:
  std::unique_ptr<KeyedService> CreateWebAppProvider(Profile* profile);

  // Invokes OpenApplication() using the test's Profile. If |wait_for_load| is
  // true, returns after the application finishes loading. Otherwise, returns
  // immediately. If |browser| is not nullptr, it will store the Browser* that
  // hosts the launched application.
  content::WebContents* LaunchApp(apps::AppLaunchParams&& params,
                                  bool wait_for_load,
                                  Browser** out_browser);
};

enum class InstallationType { kManifestInstall, kWebAppInfoInstall };

using SystemWebAppManagerTestParams = std::tuple<TestProfileType>;

class SystemWebAppManagerBrowserTest
    : public SystemWebAppBrowserTestBase,
      public ::testing::WithParamInterface<SystemWebAppManagerTestParams> {
 public:
  explicit SystemWebAppManagerBrowserTest(bool install_mock = true);
  ~SystemWebAppManagerBrowserTest() override = default;

  TestProfileType profile_type() const { return std::get<0>(GetParam()); }

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

std::string SystemWebAppManagerTestParamsToString(
    const ::testing::TestParamInfo<SystemWebAppManagerTestParams>& param_info);

}  // namespace web_app

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(SUITE, PARAMS) \
  INSTANTIATE_TEST_SUITE_P(All, SUITE, PARAMS,                         \
                           web_app::SystemWebAppManagerTestParamsToString)

// Instantiates 3 versions of each test in |SUITE| to ensure coverage of
// Guest and Incognito profiles, as well as regular profiles. This is currently
// only used on ChromeOS. Other platforms will likely need a differently defined
// macro because there is no such thing as Guest mode.
#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_ALL_PROFILE_TYPES_P( \
    SUITE)                                                                 \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                         \
      SUITE,                                                               \
      ::testing::Values(TestProfileType::kRegular,                         \
                        TestProfileType::kIncognito, TestProfileType::kGuest))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                             \
      SUITE, ::testing::Values(TestProfileType::kRegular))

#define INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(SUITE) \
  INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(                           \
      SUITE, ::testing::Values(TestProfileType::kGuest))

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_SYSTEM_WEB_APP_BROWSERTEST_BASE_H_
