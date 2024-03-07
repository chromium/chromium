// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_coordinator.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace web_app {
namespace {

class SimpleInstallDialogBubbleViewBrowserTest
    : public WebAppControllerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SimpleInstallDialogBubbleViewBrowserTest()
      : prevent_close_on_deactivate_(
            web_app::SetDontCloseOnDeactivateForTesting()) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(
        feature_engagement::kIPHDesktopPwaInstallFeature);
    if (UniversalInstallEnabled()) {
      enabled_features.push_back(features::kWebAppUniversalInstall);
    } else {
      disabled_features.push_back(features::kWebAppUniversalInstall);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~SimpleInstallDialogBubbleViewBrowserTest() override = default;

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

 protected:
  views::BubbleDialogDelegate* GetBubbleView(Browser* browser) {
    return WebAppInstallDialogCoordinator::GetOrCreateForBrowser(browser)
        ->GetBubbleView();
  }

  bool UniversalInstallEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::AutoReset<bool> prevent_close_on_deactivate_;
};

IN_PROC_BROWSER_TEST_P(SimpleInstallDialogBubbleViewBrowserTest,
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
  ShowSimpleInstallDialogForWebApps(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker), base::DoNothing());
}

IN_PROC_BROWSER_TEST_P(SimpleInstallDialogBubbleViewBrowserTest,
                       CancelledDialogReportsMetrics) {
  auto app_info = GetAppInfo();

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());

  base::RunLoop loop;
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            loop.Quit();
          }));

  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleView(browser())->GetWidget());
  GetBubbleView(browser())->CancelDialog();
  loop.Run();
  destroyed_waiter.Wait();

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_P(SimpleInstallDialogBubbleViewBrowserTest,
                       AcceptDialogReportsMetrics) {
  auto app_info = GetAppInfo();

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());

  base::RunLoop loop;
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            loop.Quit();
          }));

  base::HistogramTester histogram_tester;
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleView(browser())->GetWidget());
  GetBubbleView(browser())->AcceptDialog();
  loop.Run();
  destroyed_waiter.Wait();

  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_P(SimpleInstallDialogBubbleViewBrowserTest,
                       CancelledDialogReportsIphIgnored) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url;

  base::RunLoop loop;
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ShowSimpleInstallDialogForWebApps(
      web_contents, std::move(app_info), std::move(install_tracker),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            loop.Quit();
          }),
      PwaInProductHelpState::kShown);

  GetBubbleView(browser())->CancelDialog();
  loop.Run();
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);

  EXPECT_EQ(GetIntWebAppPref(pref_service, app_id,
                             kIphPrefNames.not_accepted_count_name)
                .value(),
            1);
  EXPECT_TRUE(GetTimeWebAppPref(pref_service, app_id,
                                kIphPrefNames.last_ignore_time_name)
                  .has_value());
  {
    const auto& dict =
        pref_service->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphPrefNames.not_accepted_count_name).value_or(0),
              1);
    EXPECT_TRUE(dict.contains(kIphPrefNames.last_ignore_time_name));
  }
}

IN_PROC_BROWSER_TEST_P(SimpleInstallDialogBubbleViewBrowserTest,
                       AcceptDialogResetIphCounters) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url;
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();

  WebAppPrefGuardrails::GetForDesktopInstallIph(pref_service)
      .RecordIgnore(app_id, base::Time::Now());
  base::RunLoop loop;
  // Show the PWA install dialog.
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            loop.Quit();
          }),
      PwaInProductHelpState::kShown);

  GetBubbleView(browser())->AcceptDialog();
  loop.Run();

  EXPECT_EQ(GetIntWebAppPref(pref_service, app_id,
                             kIphPrefNames.not_accepted_count_name)
                .value(),
            0);
  {
    const auto& dict =
        pref_service->GetDict(prefs::kWebAppsAppAgnosticIphState);
    EXPECT_EQ(dict.FindInt(kIphPrefNames.not_accepted_count_name).value_or(0),
              0);
  }
}

IN_PROC_BROWSER_TEST_P(SimpleInstallDialogBubbleViewBrowserTest,
                       CancelFromNavigation) {
  std::optional<bool> dialog_accepted_ = std::nullopt;

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), GetAppInfo(),
      std::move(install_tracker),
      base::BindLambdaForTesting(
          [&](bool accepted,
              std::unique_ptr<WebAppInstallInfo> app_info_callback) {
            dialog_accepted_ = accepted;
          }));

  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroy_waiter(
      GetBubbleView(browser())->GetWidget());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  ASSERT_TRUE(dialog_accepted_);
  ASSERT_FALSE(dialog_accepted_.value());

    histograms.ExpectUniqueSample("WebApp.InstallConfirmation.CloseReason",
                                  views::Widget::ClosedReason::kUnspecified, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SimpleInstallDialogBubbleViewBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "WebAppSimpleInstallDialogUniversal"
                                      : "PWAConfirmationBubbleView";
                         });

}  // namespace
}  // namespace web_app
