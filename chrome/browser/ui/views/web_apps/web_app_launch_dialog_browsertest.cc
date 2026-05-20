// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/web_apps/web_app_dialog_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace web_app {
namespace {

class WebAppLaunchDialogBrowserTest : public InProcessBrowserTest {
 protected:
  SkBitmap MakeIcon() {
    SkBitmap bm;
    bm.allocN32Pixels(32, 32);
    bm.eraseColor(SK_ColorBLUE);
    return bm;
  }
};

// Verifies that shrinking the host popup window causes the launch dialog to
// auto-close via the OnWidgetBoundsChanged size-occlusion guard.
IN_PROC_BROWSER_TEST_F(WebAppLaunchDialogBrowserTest,
                       WindowSizeLoweringClosesDialog) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open a popup and capture the new Browser* for it.
  ui_test_utils::BrowserCreatedObserver browser_observer;
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      embedded_test_server()->GetURL("/empty.html"),
                      /*width=*/500, /*height=*/500);
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  ASSERT_TRUE(content::WaitForLoadStop(popup_contents));
  Browser* popup_browser = browser_observer.Wait();
  ASSERT_NE(popup_browser, nullptr);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebInstallLaunchDialog");
  base::test::TestFuture<bool> dialog_future;
  ShowWebInstallAppLaunchDialog(
      popup_contents, webapps::AppId("test_app"),
      Profile::FromBrowserContext(popup_contents->GetBrowserContext()),
      "Test Application", MakeIcon(), dialog_future.GetCallback());

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(dialog_future.IsReady());

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Make the size of the popup window too small for the dialog.
  ui_test_utils::SetAndWaitForBounds(*popup_browser, gfx::Rect(100, 100));
  destroy_waiter.Wait();

  ASSERT_TRUE(dialog_future.Wait());
  EXPECT_FALSE(dialog_future.Get());
}

// Verifies that showing the launch dialog in a window that is already too small
// causes it to close immediately via the post-show size check.
IN_PROC_BROWSER_TEST_F(WebAppLaunchDialogBrowserTest,
                       SmallWindowClosesDialogAutomatically) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  ASSERT_TRUE(content::WaitForLoadStop(popup_contents));

  views::AnyWidgetObserver widget_observer(views::test::AnyWidgetTestPasskey{});

  base::RunLoop run_loop;
  widget_observer.set_closing_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget->GetName() == "WebInstallLaunchDialog") {
          run_loop.Quit();
        }
      }));

  base::test::TestFuture<bool> dialog_future;
  ShowWebInstallAppLaunchDialog(
      popup_contents, webapps::AppId("test_app"),
      Profile::FromBrowserContext(popup_contents->GetBrowserContext()),
      "Test Application", MakeIcon(), dialog_future.GetCallback());

  run_loop.Run();

  ASSERT_TRUE(dialog_future.Wait());
  EXPECT_FALSE(dialog_future.Get());
}

}  // namespace
}  // namespace web_app
