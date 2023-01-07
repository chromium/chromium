// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"

class PWAConfirmationBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  PWAConfirmationBubbleViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {feature_engagement::kIPHDesktopPwaInstallFeature}, {});
  }
  ~PWAConfirmationBubbleViewBrowserTest() override = default;

  std::unique_ptr<WebAppInstallInfo> GetAppInfo() {
    auto app_info = std::make_unique<WebAppInstallInfo>();
    app_info->title = u"Test app 2";
    app_info->start_url = GURL("https://example2.com");
    app_info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    return app_info;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       ShowBubbleInPWAWindow) {
  auto app_info = std::make_unique<WebAppInstallInfo>();
  app_info->title = u"Test app";
  app_info->start_url = GURL("https://example.com");
  Profile* profile = browser()->profile();
  web_app::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(app_info));
  Browser* browser = web_app::LaunchWebAppBrowser(profile, app_id);

  app_info = GetAppInfo();
  // Tests that we don't crash when showing the install prompt in a PWA window.
  chrome::ShowPWAInstallBubble(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::DoNothing());

  // Tests that we don't crash when attempting to show bubble when it's already
  // shown.
  app_info = std::make_unique<WebAppInstallInfo>();
  app_info->title = u"Test app 3";
  app_info->start_url = GURL("https://example3.com");
  app_info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  chrome::ShowPWAInstallBubble(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::DoNothing());
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
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            loop.Quit();
          }));

  PWAConfirmationBubbleView* bubble_dialog =
      PWAConfirmationBubbleView::GetBubble();

  base::HistogramTester histograms;
  bubble_dialog->CancelDialog();
  loop.Run();

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       CancelledDialogReportsIphIgnored) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url;
  base::RunLoop loop;
  // Show the PWA install dialog.
  chrome::ShowPWAInstallBubble(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            loop.Quit();
          }),
      chrome::PwaInProductHelpState::kShown);

  PWAConfirmationBubbleView* bubble_dialog =
      PWAConfirmationBubbleView::GetBubble();

  bubble_dialog->CancelDialog();
  loop.Run();
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  web_app::AppId app_id =
      web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  EXPECT_EQ(
      web_app::GetIntWebAppPref(pref_service, app_id, web_app::kIphIgnoreCount)
          .value(),
      1);
  EXPECT_TRUE(web_app::GetTimeWebAppPref(pref_service, app_id,
                                         web_app::kIphLastIgnoreTime)
                  .has_value());
  {
    const auto& dict =
        pref_service->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(web_app::kIphIgnoreCount).value_or(0), 1);
    EXPECT_TRUE(dict.contains(web_app::kIphLastIgnoreTime));
  }
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       AcceptDialogResetIphCounters) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url;
  web_app::AppId app_id =
      web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  web_app::UpdateIntWebAppPref(pref_service, app_id, web_app::kIphIgnoreCount,
                               1);
  {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticIphState);
    update->Set(web_app::kIphIgnoreCount, 1);
  }
  base::RunLoop loop;
  // Show the PWA install dialog.
  chrome::ShowPWAInstallBubble(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            loop.Quit();
          }),
      chrome::PwaInProductHelpState::kShown);

  PWAConfirmationBubbleView* bubble_dialog =
      PWAConfirmationBubbleView::GetBubble();

  bubble_dialog->AcceptDialog();
  loop.Run();

  EXPECT_EQ(
      web_app::GetIntWebAppPref(pref_service, app_id, web_app::kIphIgnoreCount)
          .value(),
      0);
  {
    const auto& dict =
        pref_service->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(web_app::kIphIgnoreCount).value_or(0), 0);
  }
}
