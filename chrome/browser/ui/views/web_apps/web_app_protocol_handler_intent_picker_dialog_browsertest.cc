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
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
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

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    WebAppProtocolHandlerIntentPickerDialog_EscapeDoesNotRememberPreference) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppProtocolHandlerAcceptanceCallback>
      show_dialog;
  EXPECT_CALL(show_dialog,
              Run(/*allowed=*/false, /*remember_user_choice=*/false));

  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      browser()->profile(),
      ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppProtocolHandlerIntentPickerView::SetDefaultRememberSelectionForTesting(
      true);
  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), test_app_id,
      std::move(profile_keep_alive), std::move(keep_alive), show_dialog.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  WebAppProtocolHandlerIntentPickerView::SetDefaultRememberSelectionForTesting(
      false);
}

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    ProtocolHandlerIntentPickerDialog_DisallowAndRemember) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppProtocolHandlerAcceptanceCallback>
      show_dialog;
  EXPECT_CALL(show_dialog,
              Run(/*allowed=*/false, /*remember_user_choice=*/true));

  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      browser()->profile(),
      ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppProtocolHandlerIntentPickerView::SetDefaultRememberSelectionForTesting(
      true);
  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), test_app_id,
      std::move(profile_keep_alive), std::move(keep_alive), show_dialog.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
  WebAppProtocolHandlerIntentPickerView::SetDefaultRememberSelectionForTesting(
      false);
}

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    ProtocolHandlerIntentPickerDialog_DisallowDoNotRemember) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppProtocolHandlerAcceptanceCallback>
      show_dialog;
  EXPECT_CALL(show_dialog,
              Run(/*allowed=*/false, /*remember_user_choice=*/false));

  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      browser()->profile(),
      ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), test_app_id,
      std::move(profile_keep_alive), std::move(keep_alive), show_dialog.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
}

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    ProtocolHandlerIntentPickerDialog_AcceptAndRemember) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppProtocolHandlerAcceptanceCallback>
      show_dialog;
  EXPECT_CALL(show_dialog,
              Run(/*allowed=*/true, /*remember_user_choice=*/true));

  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      browser()->profile(),
      ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);

  WebAppProtocolHandlerIntentPickerView::SetDefaultRememberSelectionForTesting(
      true);
  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), test_app_id,
      std::move(profile_keep_alive), std::move(keep_alive), show_dialog.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  WebAppProtocolHandlerIntentPickerView::SetDefaultRememberSelectionForTesting(
      false);
}

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    ProtocolHandlerIntentPickerDialog_AcceptDoNotRemember) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppProtocolHandlerAcceptanceCallback>
      show_dialog;
  EXPECT_CALL(show_dialog,
              Run(/*allowed=*/true, /*remember_user_choice=*/false));

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
