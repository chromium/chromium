// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_protocol_handler_intent_picker_dialog_view.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

web_app::AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = u"Test app";
  app_info->start_url = example_url;
  app_info->scope = example_url;
  apps::ProtocolHandlerInfo protocol_handler;
  protocol_handler.protocol = "web+test";
  protocol_handler.url = GURL("http://example.org/?uri=%s");
  app_info->protocol_handlers.push_back(std::move(protocol_handler));
  return web_app::test::InstallWebApp(profile, std::move(app_info));
}

}  // namespace

class WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest
    : public InProcessBrowserTest {};

// TODO(crbug.com/1105257): Add more tests for the actual user flow when we
// hook up the dialog with the ProtocolHandlerRegistry.
IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    ShowWebAppProtocolHandlerIntentPickerDialog) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<base::OnceCallback<void(bool)>> show_dialog;
  EXPECT_CALL(show_dialog, Run(false));

  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      browser()->profile(),
      ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), test_app_id,
      std::move(profile_keep_alive), std::move(keep_alive), show_dialog.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
}

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    AcceptProtocolHandlerIntentPickerDialog) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<base::OnceCallback<void(bool)>> show_dialog;
  EXPECT_CALL(show_dialog, Run(true));

  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      browser()->profile(),
      ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), test_app_id,
      std::move(profile_keep_alive), std::move(keep_alive), show_dialog.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
}

class WebAppProtocolHandlerIntentPickerDialogInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        "WebAppProtocolHandlerIntentPickerView");
    GURL protocol_url("web+test://test");
    web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        browser()->profile(),
        ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::WEB_APP_INTENT_PICKER,
        KeepAliveRestartOption::DISABLED);
    WebAppProtocolHandlerIntentPickerView::Show(
        protocol_url, browser()->profile(), test_app_id,
        std::move(profile_keep_alive), std::move(keep_alive),
        base::DoNothing());
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kEscKeyPressed);
  }
};

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInteractiveBrowserTest,
    InvokeUi_CloseDialog) {
  ShowAndVerifyUi();
}
