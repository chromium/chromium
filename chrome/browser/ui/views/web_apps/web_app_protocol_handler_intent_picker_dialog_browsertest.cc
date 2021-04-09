// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_protocol_handler_intent_picker_dialog_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

web_app::AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->title = base::UTF8ToUTF16("Test app");
  app_info->start_url = example_url;
  app_info->scope = example_url;
  blink::Manifest::ProtocolHandler protocol_handler;
  protocol_handler.protocol = base::UTF8ToUTF16("web+test");
  protocol_handler.url = GURL("http://example.org/?uri=%s");
  app_info->protocol_handlers.push_back(std::move(protocol_handler));
  return web_app::InstallWebApp(profile, std::move(app_info));
}

}  // namespace

class WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest
    : public InProcessBrowserTest {};

// TODO(crbug.com/1105257): Add more tests for the actual user flow when we
// hook up the dialog with the ProtocolHandlerRegistry.
// TODO(crbug.com/1105257): Disabled due to testing a dialog with string
// resources that is not used in production. The string resources are not loaded
// in testing environment and crashes the test.
IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    DISABLED_ShowWebAppProtocolHandlerIntentPickerDialog) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GURL protocol_url("web+test://test");
  std::vector<std::string> app_ids;

  base::MockCallback<base::OnceCallback<void(bool)>> show_dialog;
  EXPECT_CALL(show_dialog, Run(false));

  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), command_line, app_ids,
      show_dialog.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
}

// TODO(crbug.com/1105257): Disabled due to testing a dialog with string
// resources that is not used in production. The string resources are not loaded
// in testing environment and crashes the test.
IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInProcessBrowserTest,
    DISABLED_AcceptProtocolHandlerIntentPickerDialog) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppProtocolHandlerIntentPickerView");
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GURL protocol_url("web+test://test");
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());
  std::vector<std::string> app_ids = {test_app_id};

  base::MockCallback<base::OnceCallback<void(bool)>> show_dialog;
  EXPECT_CALL(show_dialog, Run(true));

  WebAppProtocolHandlerIntentPickerView::Show(
      protocol_url, browser()->profile(), command_line, app_ids,
      show_dialog.Get());

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
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    GURL protocol_url("web+test://test");
    web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());
    std::vector<std::string> app_ids = {test_app_id};
    WebAppProtocolHandlerIntentPickerView::Show(
        protocol_url, browser()->profile(), command_line, app_ids,
        base::DoNothing());
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kEscKeyPressed);
  }
};

// TODO(crbug.com/1105257): Disabled due to testing a dialog with string
// resources that is not used in production. The string resources are not loaded
// in testing environment and crashes the test.
IN_PROC_BROWSER_TEST_F(
    WebAppProtocolHandlerIntentPickerDialogInteractiveBrowserTest,
    DISABLED_InvokeUi_CloseDialog) {
  ShowAndVerifyUi();
}
