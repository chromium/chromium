// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_dialog_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace web_app {

namespace {

static constexpr int kIconSize = 40;
static constexpr int kScreenshotSize = 300;
static constexpr SkColor kIconColor = SK_ColorGREEN;

SkBitmap CreateSolidColorIcon(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return bitmap;
}

std::unique_ptr<WebAppInstallInfo> GetInstallInfo() {
  auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com"));
  install_info->title = u"test";
  install_info->description = u"This is a test app";

  install_info->icon_bitmaps.any[kIconSize] =
      CreateSolidColorIcon(kIconSize, kIconSize, kIconColor);
  return install_info;
}

std::vector<webapps::Screenshot> GetScreenshots(const std::string& type) {
  std::vector<webapps::Screenshot> screenshots;
  if (type == "single_screenshot") {
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
        u"example screenshot");
  } else if (type == "multiple_screenshots") {
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
        u"example screenshot");
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorBLACK),
        u"example screenshot 2");
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorBLUE),
        u"");
  } else if (type == "max_ratio_screenshot") {
    screenshots.emplace_back(
        CreateSolidColorIcon(webapps::kMaximumScreenshotRatio * kScreenshotSize,
                             kScreenshotSize, SK_ColorGREEN),
        std::nullopt);
  } else {
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
        std::nullopt);
  }

  return screenshots;
}

std::unique_ptr<webapps::MlInstallOperationTracker> GetMLInstallTracker(
    Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  return webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
      ->RegisterCurrentInstallForWebContents(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
}

class WebAppDetailedInstallDialogBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowWebAppDetailedInstallDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0), GetInstallInfo(),
        GetMLInstallTracker(browser()),
        base::BindLambdaForTesting(
            [&](bool result, std::unique_ptr<WebAppInstallInfo>) {
              dialog_accepted_ = result;
            }),
        GetScreenshots(name), PwaInProductHelpState::kNotShown);
  }
  std::optional<bool> dialog_accepted() { return dialog_accepted_; }

 private:
  std::optional<bool> dialog_accepted_ = std::nullopt;
};

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_single_screenshot) {
  ShowAndVerifyUi();
  // `ShowAndVerifyUi` close the dialog without accept/cancel.
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_multiple_screenshots) {
  ShowAndVerifyUi();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_max_ratio_screenshot) {
  ShowAndVerifyUi();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_navigation) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_navigation");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_invisible) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_web_contents_invisible");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Navigate to a new tab.
  content::WebContents::CreateParams params(browser()->profile());
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(params), /*foreground=*/true);

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_destroyed) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_web_contents_destroyed");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  content::WebContents::CreateParams params(browser()->profile());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Close();

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_accept_dialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("accept_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  ASSERT_TRUE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_cancel_dialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("cancel_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       WindowSizeLoweringClosesDialog) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://www.example.com"),
                      /*width=*/500, /*height=*/500);
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  Browser* popup_browser = chrome::FindBrowserWithTab(popup_contents);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetMLInstallTracker(popup_browser);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  ShowWebAppDetailedInstallDialog(
      popup_browser->tab_strip_model()->GetActiveWebContents(),
      GetInstallInfo(), std::move(install_tracker), test_future.GetCallback(),
      GetScreenshots(base::EmptyString()), PwaInProductHelpState::kNotShown);

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

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       SmallPopupClosesWindowAutomatically) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://www.example.com"));
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  Browser* popup_browser = chrome::FindBrowserWithTab(popup_contents);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetMLInstallTracker(popup_browser);

  base::HistogramTester histograms;
  views::AnyWidgetObserver widget_observer(views::test::AnyWidgetTestPasskey{});

  base::RunLoop run_loop;
  widget_observer.set_closing_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget->GetName() == "WebAppDetailedInstallDialog") {
          run_loop.Quit();
        }
      }));
  ShowWebAppDetailedInstallDialog(popup_contents, GetInstallInfo(),
                                  std::move(install_tracker), base::DoNothing(),
                                  GetScreenshots(base::EmptyString()),
                                  PwaInProductHelpState::kNotShown);
  run_loop.Run();

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

class PictureInPictureDetailedInstallDialogOcclusionTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void ShowDialogUi() {
    ShowWebAppDetailedInstallDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0), GetInstallInfo(),
        GetMLInstallTracker(browser()), base::DoNothing(),
        GetScreenshots(base::EmptyString()), PwaInProductHelpState::kNotShown);
  }
  DocumentPictureInPictureMixinTestBase picture_in_picture_test_base_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(PictureInPictureDetailedInstallDialogOcclusionTest,
                       PipWindowCloses) {
  picture_in_picture_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser());
  auto* pip_web_contents =
      picture_in_picture_test_base_.window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_test_base_.WaitForPageLoad(pip_web_contents);

  // Show dialog.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowDialogUi();
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  EXPECT_NE(nullptr, widget);

  // Occlude dialog with picture in picture web contents, verify window is
  // closed but dialog stays open.
  PictureInPictureWindowManager::GetInstance()
      ->GetOcclusionTracker()
      ->SetWidgetOcclusionStateForTesting(widget, /*occluded=*/true);
  EXPECT_TRUE(picture_in_picture_test_base_.AwaitPipWindowClosedSuccessfully());
  EXPECT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
}

}  // namespace

}  // namespace web_app
