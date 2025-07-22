// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_update_review_dialog.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_update_identity_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/root_view.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class WebAppUpdateReviewDialog : public DialogBrowserTest {
 public:
  const GURL kTestUrl = GURL("http://www.test.com");
  WebAppUpdateReviewDialog() = default;
  WebAppUpdateReviewDialog(const WebAppUpdateReviewDialog&) = delete;
  WebAppUpdateReviewDialog& operator=(const WebAppUpdateReviewDialog&) = delete;

  void SetUpOnMainThread() override {
    old_icon_ =
        ::shortcuts::GenerateBitmap(WebAppUpdateIdentityView::kLogoSize, u"A");
    new_icon_ =
        ::shortcuts::GenerateBitmap(WebAppUpdateIdentityView::kLogoSize, u"D");

    std::unique_ptr<WebAppInstallInfo> web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kTestUrl);
    web_app_info->title = u"Abc";
    web_app_info->icon_bitmaps.any[WebAppUpdateIdentityView::kLogoSize] =
        old_icon_;
    app_id_ = web_app::test::InstallWebApp(browser()->profile(),
                                           std::move(web_app_info));
    update_.old_title = u"Abc";
    update_.old_icon = gfx::Image::CreateFrom1xBitmap(old_icon_);
    update_.old_start_url = kTestUrl;
  }

  void TearDownOnMainThread() override { provider_ = nullptr; }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Modify the update configuration based on the test name.
    if (base::Contains(name, "NameChange")) {
      update_.new_title =
          u"Definitely a longer title that is really really really really "
          u"long.";
    }
    if (base::Contains(name, "IconChange")) {
      update_.new_icon = gfx::Image::CreateFrom1xBitmap(new_icon_);
    }
    if (base::Contains(name, "UrlChange")) {
      update_.new_start_url = GURL("http://other.test.com");
    }

    web_app::ShowWebAppReviewUpdateDialog(app_id_, update_, browser(),
                                          dialog_result_.GetCallback());
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi()) {
      return false;
    }
    bool is_showing =
        browser()->GetBrowserView().GetProperty(kIsPwaUpdateDialogShowingKey);
    EXPECT_TRUE(is_showing);
    return is_showing;
  }

 protected:
  raw_ptr<web_app::WebAppProvider> provider_ = nullptr;
  SkBitmap old_icon_;
  SkBitmap new_icon_;

  WebAppIdentityUpdate update_;
  std::string app_id_;

  base::test::TestFuture<WebAppIdentityUpdateResult> dialog_result_;
};

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, InvokeUi_NameChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_NameChange_IconChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_NameChange_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_NameChange_IconChange_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, InvokeUi_IconChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       InvokeUi_IconChange_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, InvokeUi_UrlChange) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog,
                       CloseUpdateReviewDialogOnUninstall) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  ShowUi("NameChange");
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget != nullptr);
  ASSERT_FALSE(dialog_widget->IsClosed());

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::RunLoop run_loop;
  observer.set_closing_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget == dialog_widget) {
          run_loop.Quit();
        }
      }));
  // Uninstalling the app will close the update dialog
  web_app::test::UninstallWebApp(browser()->profile(), app_id_);
  run_loop.Run();

  EXPECT_FALSE(
      browser()->GetBrowserView().GetProperty(kIsPwaUpdateDialogShowingKey));
  EXPECT_EQ(dialog_result_.Get(),
            WebAppIdentityUpdateResult::kAppUninstalledDuringDialog);
}

IN_PROC_BROWSER_TEST_F(WebAppUpdateReviewDialog, ShowWhileAlreadyShowing) {
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey(), "WebAppUpdateReviewDialog");
  ShowUi("NameChange");
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget != nullptr);
  ASSERT_FALSE(dialog_widget->IsClosed());

  base::test::TestFuture<WebAppIdentityUpdateResult> update_result;
  web_app::ShowWebAppReviewUpdateDialog(app_id_, update_, browser(),
                                        update_result.GetCallback());
  EXPECT_TRUE(update_result.Wait());
  EXPECT_EQ(update_result.Get(), WebAppIdentityUpdateResult::kUnexpectedError);

  dialog_widget->Close();
}

}  // namespace
}  // namespace web_app
