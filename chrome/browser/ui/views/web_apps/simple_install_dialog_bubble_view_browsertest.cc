// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_dialog_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
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
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace web_app {
namespace {

// Creates a dummy WebAppInstallInfo instance used to populate details on the
// install dialog.
std::unique_ptr<WebAppInstallInfo> GetAppInfo() {
  auto app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example2.com"));
  app_info->title = u"Test app 2";
  app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  return app_info;
}

// Creates an installation tracker for ML installability promoter required by
// the install dialog.
std::unique_ptr<webapps::MlInstallOperationTracker> GetInstallTracker(
    Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  return webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
      ->RegisterCurrentInstallForWebContents(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
}

constexpr char kInstallDialogName[] = "WebAppSimpleInstallDialog";

class SimpleInstallDialogBubbleViewBrowserTest : public WebAppBrowserTestBase {
 public:
  SimpleInstallDialogBubbleViewBrowserTest()
      : prevent_close_on_deactivate_(
            web_app::SetDontCloseOnDeactivateForTesting()) {}
  ~SimpleInstallDialogBubbleViewBrowserTest() override = default;

 private:
  base::AutoReset<bool> prevent_close_on_deactivate_;
};

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       ShowBubbleInPWAWindow) {
  Profile* profile = browser()->profile();
  webapps::AppId app_id = test::InstallDummyWebApp(profile, "Test app",
                                                   GURL("https://example.com"));
  Browser* browser = ::web_app::LaunchWebAppBrowser(profile, app_id);
  auto app_info = GetAppInfo();
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser);

  // Tests that we don't crash when showing the install prompt in a PWA window.
  ShowSimpleInstallDialogForWebApps(
      browser->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker), base::DoNothing());
}

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       CancelledDialogReportsMetrics) {
  auto app_info = GetAppInfo();
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker), test_future.GetCallback());

  // Wait for the dialog to show up.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(test_future.IsReady());

  // Wait for the dialog to be destroyed post cancellation.
  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  views::test::CancelDialog(widget);
  destroyed_waiter.Wait();
  ASSERT_TRUE(test_future.Wait());
  EXPECT_FALSE(test_future.Get<bool>());

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       AcceptDialogReportsMetrics) {
  auto app_info = GetAppInfo();
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker), test_future.GetCallback());

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(test_future.IsReady());

  // Wait for the dialog to be destroyed post acceptance.
  base::HistogramTester histogram_tester;
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  views::test::AcceptDialog(widget);
  destroyed_waiter.Wait();
  ASSERT_TRUE(test_future.Wait());
  EXPECT_TRUE(test_future.Get<bool>());

  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       CancelledDialogReportsIphIgnored) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url();
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  ShowSimpleInstallDialogForWebApps(
      web_contents, std::move(app_info), std::move(install_tracker),
      test_future.GetCallback(), PwaInProductHelpState::kShown);

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(test_future.IsReady());

  base::HistogramTester histogram_tester;
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  views::test::CancelDialog(widget);
  destroyed_waiter.Wait();

  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);

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

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       AcceptDialogResetIphCounters) {
  auto app_info = GetAppInfo();
  GURL start_url = app_info->start_url();
  webapps::AppId app_id =
      GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);
  PrefService* pref_service =
      Profile::FromBrowserContext(browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetBrowserContext())
          ->GetPrefs();
  WebAppPrefGuardrails::GetForDesktopInstallIph(pref_service)
      .RecordIgnore(app_id, base::Time::Now());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(app_info),
      std::move(install_tracker), test_future.GetCallback(),
      PwaInProductHelpState::kShown);

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(test_future.IsReady());

  base::HistogramTester histogram_tester;
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  views::test::AcceptDialog(widget);
  destroyed_waiter.Wait();

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

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       CancelFromNavigation) {
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(browser());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), GetAppInfo(),
      std::move(install_tracker), test_future.GetCallback());

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(test_future.IsReady());

  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);
  destroy_waiter.Wait();

  ASSERT_TRUE(test_future.Wait());
  EXPECT_FALSE(test_future.Get<bool>());

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       WindowSizeLoweringClosesDialog) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://www.example.com"),
                      /*width=*/500, /*height=*/500);
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  Browser* popup_browser = chrome::FindBrowserWithTab(popup_contents);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(popup_browser);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  ShowSimpleInstallDialogForWebApps(
      popup_browser->tab_strip_model()->GetActiveWebContents(), GetAppInfo(),
      std::move(install_tracker), test_future.GetCallback());

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(test_future.IsReady());

  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Make the size of the popup window to be too small for the dialog.
  ui_test_utils::SetAndWaitForBounds(*popup_browser, gfx::Rect(100, 100));
  destroy_waiter.Wait();

  ASSERT_TRUE(test_future.Wait());
  EXPECT_FALSE(test_future.Get<bool>());

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(SimpleInstallDialogBubbleViewBrowserTest,
                       SmallPopupClosesWindowAutomatically) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://www.example.com"));
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  Browser* popup_browser = chrome::FindBrowserWithTab(popup_contents);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(popup_browser);

  base::HistogramTester histograms;
  views::AnyWidgetObserver widget_observer(views::test::AnyWidgetTestPasskey{});

  base::RunLoop run_loop;
  widget_observer.set_closing_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget->GetName() == kInstallDialogName) {
          run_loop.Quit();
        }
      }));
  ShowSimpleInstallDialogForWebApps(popup_contents, GetAppInfo(),
                                    std::move(install_tracker),
                                    base::DoNothing());
  run_loop.Run();

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

class PictureInPictureSimpleInstallDialogOcclusionTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  DocumentPictureInPictureMixinTestBase picture_in_picture_test_base_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(PictureInPictureSimpleInstallDialogOcclusionTest,
                       PipWindowCloses) {
  picture_in_picture_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser());
  auto* pip_web_contents =
      picture_in_picture_test_base_.window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_test_base_.WaitForPageLoad(pip_web_contents);

  // Show dialog.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, kInstallDialogName);
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  ShowSimpleInstallDialogForWebApps(
      browser()->tab_strip_model()->GetActiveWebContents(), GetAppInfo(),
      GetInstallTracker(browser()), test_future.GetCallback());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  EXPECT_NE(nullptr, widget);
  EXPECT_FALSE(test_future.IsReady());

  // Occlude dialog with picture in picture web contents, verify window is
  // closed but dialog stays open.
  PictureInPictureWindowManager::GetInstance()
      ->GetOcclusionTracker()
      ->SetWidgetOcclusionStateForTesting(widget, /*occluded=*/true);
  EXPECT_TRUE(picture_in_picture_test_base_.AwaitPipWindowClosedSuccessfully());
  EXPECT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());

  // Verify that the callback has not run yet, which is a measure that the
  // widget is still open.
  EXPECT_FALSE(test_future.IsReady());
}

}  // namespace
}  // namespace web_app
