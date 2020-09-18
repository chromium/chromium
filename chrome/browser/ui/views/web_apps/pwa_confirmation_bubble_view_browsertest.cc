// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/checkbox.h"

class PWAConfirmationBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  PWAConfirmationBubbleViewBrowserTest() {
    // Tests will crash if kDesktopPWAsRunOnOsLogin feature flag is not enabled.
    // AcceptBubbleInPWAWindowRunOnOsLoginChecked and
    // AcceptBubbleInPWAWindowRunOnOsLoginUnchecked tests interact with the
    // checkbox which is only added if feature flag is enabled.
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsRunOnOsLogin);
  }
  ~PWAConfirmationBubbleViewBrowserTest() override = default;

  std::unique_ptr<WebApplicationInfo> GetAppInfo() {
    auto app_info = std::make_unique<WebApplicationInfo>();
    app_info->title = base::UTF8ToUTF16("Test app 2");
    app_info->start_url = GURL("https://example2.com");
    app_info->open_as_window = true;
    return app_info;
  }

  std::unique_ptr<WebApplicationInfo> GetCallbackAppInfoFromDialog(
      bool run_on_os_login_checked) {
    std::unique_ptr<WebApplicationInfo> resulting_app_info = nullptr;
    auto app_info = GetAppInfo();

    base::RunLoop loop;
    // Show the PWA install dialog.
    chrome::ShowPWAInstallBubble(
        browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(app_info),
        base::BindLambdaForTesting(
            [&](bool accepted,
                std::unique_ptr<WebApplicationInfo> app_info_callback) {
              resulting_app_info = std::move(app_info_callback);
              loop.Quit();
            }));

    // Get bubble dialog, set checkbox and accept.
    PWAConfirmationBubbleView* bubble_dialog =
        PWAConfirmationBubbleView::GetBubbleForTesting();
    bubble_dialog->GetRunOnOsLoginCheckboxForTesting()->SetChecked(
        run_on_os_login_checked);
    bubble_dialog->Accept();

    loop.Run();

    return resulting_app_info;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       ShowBubbleInPWAWindow) {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = base::UTF8ToUTF16("Test app");
  app_info->start_url = GURL("https://example.com");
  Profile* profile = browser()->profile();
  web_app::AppId app_id = web_app::InstallWebApp(profile, std::move(app_info));
  Browser* browser = web_app::LaunchWebAppBrowser(profile, app_id);

  app_info = GetAppInfo();
  // Tests that we don't crash when showing the install prompt in a PWA window.
  chrome::ShowPWAInstallBubble(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::DoNothing());

  // Tests that we don't crash when attempting to show bubble when it's already
  // shown.
  app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = base::UTF8ToUTF16("Test app 3");
  app_info->start_url = GURL("https://example3.com");
  app_info->open_as_window = true;
  chrome::ShowPWAInstallBubble(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::DoNothing());
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       AcceptBubbleInPWAWindowRunOnOsLoginChecked) {
  auto resulting_app_info =
      GetCallbackAppInfoFromDialog(/*run_on_os_login_checked=*/true);
  EXPECT_TRUE(resulting_app_info->run_on_os_login);
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       AcceptBubbleInPWAWindowRunOnOsLoginUnchecked) {
  auto resulting_app_info =
      GetCallbackAppInfoFromDialog(/*run_on_os_login_checked=*/false);
  EXPECT_FALSE(resulting_app_info->run_on_os_login);
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       CancelledDialogReportsMetrics) {
  auto app_info = GetAppInfo();
  base::RunLoop loop;
  // Show the PWA install dialog.
  chrome::ShowPWAInstallBubble(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebApplicationInfo> app_info_callback) {
            loop.Quit();
          }));

  PWAConfirmationBubbleView* bubble_dialog =
      PWAConfirmationBubbleView::GetBubbleForTesting();

  base::HistogramTester histograms;
  bubble_dialog->CancelDialog();
  loop.Run();

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked, 1);
}
