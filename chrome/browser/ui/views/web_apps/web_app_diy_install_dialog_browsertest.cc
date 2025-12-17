// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_dialog_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

class WebAppDiyInstallDialogBrowserTest : public DialogBrowserTest {
 public:
  // Creates a dummy WebAppInstallInfo instance used to populate details on the
  // install dialog.
  std::unique_ptr<WebAppInstallInfo> GetAppInfo(const std::string& name) {
    auto app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.com"));

    if (name == "name_with_spaces") {
      app_info->title = u"       trimmed app    ";
    } else if (name != "empty_name") {
      app_info->title = u"test";
    }

    app_info->description = u"This is a test app";
    app_info->is_diy_app = true;
    return app_info;
  }

  // DialogBrowserTest overrides:
  void ShowUi(const std::string& name) override {
    auto install_info = GetAppInfo(name);

    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
        GetInstallTracker(browser());

    ShowDiyAppInstallDialog(browser()->tab_strip_model()->GetWebContentsAt(0),
                            std::move(install_info), std::move(install_tracker),
                            std::move(install_callback_),
                            PwaInProductHelpState::kNotShown);
  }

  // Creates an installation tracker for ML installability promoter required by
  // the install dialog.
  std::unique_ptr<webapps::MlInstallOperationTracker> GetInstallTracker(
      Browser* browser) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
        ->RegisterCurrentInstallForWebContents(
            webapps::WebappInstallSource::MENU_BROWSER_TAB);
  }

  void OverrideDialogCallback(
      AppInstallationAcceptanceCallback install_callback) {
    install_callback_ = std::move(install_callback);
  }

 private:
  AppInstallationAcceptanceCallback install_callback_ = base::DoNothing();
};

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest, InvokeUiBasic) {
  base::UserActionTester action_tester;
  ShowAndVerifyUi();
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDiyInstallShown"));
}

// Dialog destruction due to navigations or other reasons are measured as
// cancellations from an user action perspective.
IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_navigation) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  ShowUi("destroyed_on_navigation");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDiyInstallCancelled"));
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_invisible) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  ShowUi("destroyed_on_web_contents_invisible");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Navigate to a new tab.
  content::WebContents::CreateParams params(browser()->profile());
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(params), /*foreground=*/true);

  destroy_waiter.Wait();
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDiyInstallCancelled"));
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_destroyed) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  ShowUi("destroyed_on_web_contents_destroyed");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  content::WebContents::CreateParams params(browser()->profile());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Close();

  destroy_waiter.Wait();
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDiyInstallCancelled"));
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       InvokeUi_accept_dialog) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  ShowUi("accept_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDiyInstallAccepted"));
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       InvokeUi_cancel_dialog) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  ShowUi("cancel_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDiyInstallCancelled"));
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest, InvokeUi_empty_name) {
  base::UserActionTester action_tester;
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));

  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>>
      dialog_future;
  OverrideDialogCallback(dialog_future.GetCallback());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  ShowUi("empty_name");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  EXPECT_TRUE(dialog_future.Wait());

  auto dialog_results = dialog_future.Take();
  EXPECT_TRUE(std::get<bool>(dialog_results));
  EXPECT_EQ(std::get<std::unique_ptr<WebAppInstallInfo>>(dialog_results)->title,
            u"example.com");
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       InvokeUi_name_with_spaces) {
  base::UserActionTester action_tester;
  EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));

  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>>
      dialog_future;
  OverrideDialogCallback(dialog_future.GetCallback());

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  ShowUi("name_with_spaces");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  EXPECT_TRUE(dialog_future.Wait());

  auto dialog_results = dialog_future.Take();
  EXPECT_TRUE(std::get<bool>(dialog_results));
  EXPECT_EQ(std::get<std::unique_ptr<WebAppInstallInfo>>(dialog_results)->title,
            u"trimmed app");
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       WindowSizeLoweringClosesDialog) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://www.example.com"),
                      /*width=*/500, /*height=*/500);
  content::WebContents* popup_contents = popup_value.value();
  Browser* popup_browser = chrome::FindBrowserWithTab(popup_contents);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetInstallTracker(popup_browser);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>>
      dialog_future;
  OverrideDialogCallback(dialog_future.GetCallback());
  ShowDiyAppInstallDialog(
      popup_browser->tab_strip_model()->GetActiveWebContents(),
      GetAppInfo("empty_name"), std::move(install_tracker),
      dialog_future.GetCallback());

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Make the size of the popup window to be too small for the dialog.
  ui_test_utils::SetAndWaitForBounds(*popup_browser, gfx::Rect(100, 100));
  destroy_waiter.Wait();

  ASSERT_TRUE(dialog_future.Wait());
  EXPECT_FALSE(dialog_future.Get<bool>());
  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppDiyInstallDialogBrowserTest,
                       SmallWindowClosesDialogAutomatically) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://example.com"));
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
        if (widget->GetName() == "WebAppDiyInstallDialog") {
          run_loop.Quit();
        }
      }));

  ShowDiyAppInstallDialog(popup_contents, GetAppInfo("empty_name"),
                          std::move(install_tracker), base::DoNothing());

  run_loop.Run();

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

class PictureInPictureDiyDialogOcclusionTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void ShowDialogUi() {
    auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.com"));
    install_info->title = u"test";
    install_info->description = u"This is a test app";
    install_info->is_diy_app = true;

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
        webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
            ->RegisterCurrentInstallForWebContents(
                webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

    ShowDiyAppInstallDialog(browser()->tab_strip_model()->GetWebContentsAt(0),
                            std::move(install_info), std::move(install_tracker),
                            base::DoNothing(),
                            PwaInProductHelpState::kNotShown);
  }
  DocumentPictureInPictureMixinTestBase picture_in_picture_test_base_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(PictureInPictureDiyDialogOcclusionTest,
                       PipWindowCloses) {
  picture_in_picture_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser());
  auto* pip_web_contents =
      picture_in_picture_test_base_.window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_test_base_.WaitForPageLoad(pip_web_contents);

  // Show dialog.
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDiyInstallDialog");
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
