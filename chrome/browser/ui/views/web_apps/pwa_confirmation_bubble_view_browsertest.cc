// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/web_apps/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_test.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

class PWAConfirmationBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  PWAConfirmationBubbleViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {feature_engagement::kIPHDesktopPwaInstallFeature}, {});
  }
  ~PWAConfirmationBubbleViewBrowserTest() override = default;

  std::unique_ptr<WebApplicationInfo> GetAppInfo() {
    auto app_info = std::make_unique<WebApplicationInfo>();
    app_info->title = u"Test app 2";
    app_info->start_url = GURL("https://example2.com");
    app_info->open_as_window = true;
    return app_info;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       ShowBubbleInPWAWindow) {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = u"Test app";
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
  app_info->title = u"Test app 3";
  app_info->start_url = GURL("https://example3.com");
  app_info->open_as_window = true;
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
              std::unique_ptr<WebApplicationInfo> app_info_callback) {
            loop.Quit();
          }),
      chrome::PwaInProductHelpState::kShown);

  PWAConfirmationBubbleView* bubble_dialog =
      PWAConfirmationBubbleView::GetBubbleForTesting();

  bubble_dialog->CancelDialog();
  loop.Run();
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  web_app::AppId app_id = web_app::GenerateAppIdFromURL(start_url);
  EXPECT_EQ(
      web_app::GetIntWebAppPref(pref_service, app_id, web_app::kIphIgnoreCount)
          .value(),
      1);
  EXPECT_TRUE(web_app::GetTimeWebAppPref(pref_service, app_id,
                                         web_app::kIphLastIgnoreTime)
                  .has_value());
  {
    auto* dict =
        pref_service->GetDictionary(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict->FindIntKey(web_app::kIphIgnoreCount).value_or(0), 1);
    EXPECT_NE(dict->FindKey(web_app::kIphLastIgnoreTime), nullptr);
  }
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       AcceptDialogResetIphCounters) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url;
  web_app::AppId app_id = web_app::GenerateAppIdFromURL(start_url);
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  web_app::UpdateIntWebAppPref(pref_service, app_id, web_app::kIphIgnoreCount,
                               1);
  {
    prefs::ScopedDictionaryPrefUpdate update(
        pref_service, prefs::kWebAppsAppAgnosticIphState);
    update->SetInteger(web_app::kIphIgnoreCount, 1);
  }
  base::RunLoop loop;
  // Show the PWA install dialog.
  chrome::ShowPWAInstallBubble(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebApplicationInfo> app_info_callback) {
            loop.Quit();
          }),
      chrome::PwaInProductHelpState::kShown);

  PWAConfirmationBubbleView* bubble_dialog =
      PWAConfirmationBubbleView::GetBubbleForTesting();

  bubble_dialog->AcceptDialog();
  loop.Run();

  EXPECT_EQ(
      web_app::GetIntWebAppPref(pref_service, app_id, web_app::kIphIgnoreCount)
          .value(),
      0);
  {
    auto* dict =
        pref_service->GetDictionary(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict->FindIntKey(web_app::kIphIgnoreCount).value_or(0), 0);
  }
}
