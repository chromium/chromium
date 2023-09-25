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
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace web_app {
namespace {

class PWAConfirmationBubbleViewBrowserTest
    : public WebAppControllerBrowserTest {
 public:
  PWAConfirmationBubbleViewBrowserTest()
      : prevent_close_on_deactivate_(
            PWAConfirmationBubbleView::SetDontCloseOnDeactivateForTesting()) {
    scoped_feature_list_.InitWithFeatures(
        {feature_engagement::kIPHDesktopPwaInstallFeature}, {});
  }
  ~PWAConfirmationBubbleViewBrowserTest() override = default;

  std::unique_ptr<WebAppInstallInfo> GetAppInfo() {
    auto app_info = std::make_unique<WebAppInstallInfo>();
    app_info->title = u"Test app 2";
    app_info->start_url = GURL("https://example2.com");
    app_info->manifest_id = GURL("https://example2.com");
    app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    return app_info;
  }

  std::unique_ptr<webapps::MlInstallOperationTracker> GetInstallTracker(
      Browser* browser) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
        ->RegisterCurrentInstallForWebContents(
            webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::AutoReset<bool> prevent_close_on_deactivate_;
};

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       ShowBubbleInPWAWindow) {
  auto app_info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(GURL("https://example.com")));
  app_info->title = u"Test app";
  app_info->start_url = GURL("https://example.com");
  Profile* profile = browser()->profile();
  webapps::AppId app_id = test::InstallWebApp(profile, std::move(app_info));
  Browser* browser = ::web_app::LaunchWebAppBrowser(profile, app_id);

  app_info = GetAppInfo();
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser);

  // Tests that we don't crash when showing the install prompt in a PWA window.
  chrome::ShowPWAInstallBubble(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker), base::DoNothing());
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       CancelledDialogReportsMetrics) {
  auto app_info = GetAppInfo();

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());

  base::RunLoop loop;
  // Show the PWA install dialog.
  chrome::ShowPWAInstallBubble(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker),
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
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  chrome::ShowPWAInstallBubble(
      web_contents, std::move(app_info), std::move(install_tracker),
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
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  EXPECT_EQ(GetIntWebAppPref(pref_service, app_id, kIphIgnoreCount).value(), 1);
  EXPECT_TRUE(
      GetTimeWebAppPref(pref_service, app_id, kIphLastIgnoreTime).has_value());
  {
    const auto& dict =
        pref_service->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphIgnoreCount).value_or(0), 1);
    EXPECT_TRUE(dict.contains(kIphLastIgnoreTime));
  }
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       AcceptDialogResetIphCounters) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url;
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  UpdateIntWebAppPref(pref_service, app_id, kIphIgnoreCount, 1);
  {
    ScopedDictPrefUpdate update(pref_service,
                                prefs::kWebAppsAppAgnosticIphState);
    update->Set(kIphIgnoreCount, 1);
  }
  base::RunLoop loop;
  // Show the PWA install dialog.
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  chrome::ShowPWAInstallBubble(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker),
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

  EXPECT_EQ(GetIntWebAppPref(pref_service, app_id, kIphIgnoreCount).value(), 0);
  {
    const auto& dict =
        pref_service->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphIgnoreCount).value_or(0), 0);
  }
}

IN_PROC_BROWSER_TEST_F(PWAConfirmationBubbleViewBrowserTest,
                       CancelFromNavigation) {
  absl::optional<bool> dialog_accepted_ = absl::nullopt;

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  chrome::ShowPWAInstallBubble(
      browser()->tab_strip_model()->GetActiveWebContents(), GetAppInfo(),
      std::move(install_tracker),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            dialog_accepted_ = accepted;
          }));
  PWAConfirmationBubbleView* bubble_dialog =
      PWAConfirmationBubbleView::GetBubble();

  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroy_waiter(bubble_dialog->GetWidget());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  ASSERT_TRUE(dialog_accepted_);
  ASSERT_FALSE(dialog_accepted_.value());

  histograms.ExpectUniqueSample("WebApp.InstallConfirmation.CloseReason",
                                views::Widget::ClosedReason::kUnspecified, 1);
}

}  // namespace
}  // namespace web_app
