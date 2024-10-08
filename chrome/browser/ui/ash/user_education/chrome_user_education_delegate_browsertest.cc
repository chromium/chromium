// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include <memory>
#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

// ChromeUserEducationDelegateBrowserTest --------------------------------------

// Base class for browser tests of the `ChromeUserEducationDelegate`.
class ChromeUserEducationDelegateBrowserTest
    : public ash::SystemWebAppBrowserTestBase {
 public:
  // Returns a pointer to the `delegate_` instance under test.
  ash::UserEducationDelegate* delegate() { return delegate_.get(); }

 private:
  // ash::SystemWebAppBrowserTestBase:
  void SetUpOnMainThread() override {
    ash::SystemWebAppBrowserTestBase::SetUpOnMainThread();

    // Instantiate the `delegate_` after
    // `ash::SystemWebAppBrowserTestBase::SetUpOnMainThread()` so that the
    // browser process has fully initialized.
    delegate_ = std::make_unique<ChromeUserEducationDelegate>();
  }

  // The delegate instance under test.
  std::unique_ptr<ChromeUserEducationDelegate> delegate_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `LaunchSystemWebAppAsync()` is working as intended.
IN_PROC_BROWSER_TEST_F(ChromeUserEducationDelegateBrowserTest,
                       LaunchSystemWebAppAsync) {
  // Wait for Explore app installation.
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // Attempt to launch Explore app.
  delegate()->LaunchSystemWebAppAsync(
      ash::BrowserContextHelper::Get()
          ->GetUserByBrowserContext(browser()->profile())
          ->GetAccountId(),
      ash::SystemWebAppType::HELP, apps::LaunchSource::kFromWelcomeTour,
      display::kDefaultDisplayId);

  // Expect Explore app to launch asynchronously.
  EXPECT_TRUE(base::test::RunUntil([]() {
    auto* const browser = BrowserList::GetInstance()->GetLastActive();
    return browser && web_app::AppBrowserController::IsForWebApp(
                          browser, web_app::kHelpAppId);
  }));
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromWelcomeTour",
                                      apps::DefaultAppName::kHelpApp, 1);
}
