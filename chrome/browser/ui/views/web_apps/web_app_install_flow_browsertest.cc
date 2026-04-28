// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_flow_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_app {

class WebAppInstallFlowBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppInstallFlowBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kWebAppInstallDialog);
  }

  void AdvanceToDoneAndAccept(views::Widget* widget) {
    ASSERT_TRUE(widget);
    auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
    ASSERT_TRUE(dialog_delegate);

    views::test::WidgetDestroyedWaiter waiter(widget);

    // Step 1: Install Dialog. Accept to move to options.
    dialog_delegate->AcceptDialog();

    // Step 2: Installer Options. Accept to move to progress.
    dialog_delegate->AcceptDialog();

    // Step 3: Progress. Wait for completion and accept to move to success.
    ASSERT_TRUE(base::test::RunUntil([dialog_delegate, widget]() -> bool {
      return dialog_delegate->IsDialogButtonEnabled(
                 ui::mojom::DialogButton::kOk) ||
             widget->IsClosed();
    }));
    if (!widget->IsClosed()) {
      dialog_delegate->AcceptDialog();
    }

    // Step 4: Successful. Accept to close the dialog.
    if (!widget->IsClosed()) {
      dialog_delegate->AcceptDialog();
    }
    waiter.Wait();
  }

  IconLabelBubbleView* GetPwaInstallIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    if (!browser_view || !browser_view->toolbar_button_provider()) {
      return nullptr;
    }
    return browser_view->toolbar_button_provider()->GetPageActionView(
        kActionInstallPwa);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, SimpleInstallFlow) {
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  // Wait for the omnibox icon to become visible.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* icon = GetPwaInstallIconView();
    return icon && icon->GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");

  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, DetailedInstallFlow) {
  // Detailed install flow is triggered when screenshots are available.
  GURL app_url = embedded_https_test_server().GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_screenshots.json");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* icon = GetPwaInstallIconView();
    return icon && icon->GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");

  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, DiyInstallFlow) {
  // Navigate to a page that is not installable.
  GURL app_url = embedded_https_test_server().GetURL(
      "/banners/no_manifest_test_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");

  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  // Show the dialog.
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
}

}  // namespace web_app
