// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_launch_dialog_view.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace web_app {

namespace {

webapps::AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");
  auto app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(example_url);
  app_info->title = u"Test app";
  app_info->scope = example_url;
  apps::ProtocolHandlerInfo protocol_handler;
  protocol_handler.protocol = "web+test";
  protocol_handler.url = GURL("http://example.org/?uri=%s");
  app_info->protocol_handlers.push_back(std::move(protocol_handler));
  return test::InstallWebApp(profile, std::move(app_info));
}

}  // namespace

class ProtocolHandlerLaunchDialogBrowserTest : public WebAppBrowserTestBase {
 public:
  void ShowDialogAndCloseWithReason(views::Widget::ClosedReason reason,
                                    bool expected_allowed,
                                    bool expected_remember_user_choice) {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");
    GURL protocol_url("web+test://test");
    webapps::AppId test_app_id = InstallTestWebApp(browser()->profile());

    base::RunLoop run_loop;
    auto dialog_finished = base::BindLambdaForTesting(
        [&](bool allowed, bool remember_user_choice) {
          run_loop.Quit();
          EXPECT_EQ(expected_allowed, allowed);
          EXPECT_EQ(expected_remember_user_choice, remember_user_choice);
        });

    ShowWebAppProtocolLaunchDialog(protocol_url, browser()->profile(),
                                   test_app_id, std::move(dialog_finished));

    waiter.WaitIfNeededAndGet()->CloseWithReason(reason);
    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_F(
    ProtocolHandlerLaunchDialogBrowserTest,
    WebAppProtocolHandlerIntentPickerDialog_EscapeDoesNotRememberPreference) {
  ProtocolHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(true);
  ShowDialogAndCloseWithReason(views::Widget::ClosedReason::kEscKeyPressed,
                               /*allowed=*/false,
                               /*remember_user_choice=*/false);
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogBrowserTest,
                       ProtocolHandlerIntentPickerDialog_DisallowAndRemember) {
  ProtocolHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(true);
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked,
      /*allowed=*/false,
      /*remember_user_choice=*/true);
}

IN_PROC_BROWSER_TEST_F(
    ProtocolHandlerLaunchDialogBrowserTest,
    ProtocolHandlerIntentPickerDialog_DisallowDoNotRemember) {
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked,
      /*allowed=*/false,
      /*remember_user_choice=*/false);
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogBrowserTest,
                       ProtocolHandlerIntentPickerDialog_AcceptAndRemember) {
  ProtocolHandlerLaunchDialogView::SetDefaultRememberSelectionForTesting(true);
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked,
      /*allowed=*/true,
      /*remember_user_choice=*/true);
}

IN_PROC_BROWSER_TEST_F(ProtocolHandlerLaunchDialogBrowserTest,
                       ProtocolHandlerIntentPickerDialog_AcceptDoNotRemember) {
  ShowDialogAndCloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked,
      /*allowed=*/true,
      /*remember_user_choice=*/false);
}

class WebAppProtocolHandlerIntentPickerDialogInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");
    GURL protocol_url("web+test://test");
    webapps::AppId test_app_id = InstallTestWebApp(browser()->profile());
    ShowWebAppProtocolLaunchDialog(protocol_url, browser()->profile(),
                                   test_app_id, base::DoNothing());
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kEscKeyPressed);
  }
};

IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInteractiveBrowserTest,
    InvokeUi_CloseDialog) {
  ShowAndVerifyUi();
}

}  // namespace web_app
