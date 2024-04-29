// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/payments/payment_app_install_util.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/payments/core/features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/views/controls/image_view.h"

namespace payments {
namespace {

class PaymentHandlerHeaderViewUITest : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerHeaderViewUITest() = default;
  ~PaymentHandlerHeaderViewUITest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler.html");
  }
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerHeaderViewUITest,
                       HeaderHasCorrectDetails) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest, and wait until the PaymentHandler has loaded a
  // web-contents that has set a title.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a SHEET_TITLE, we have to explicitly test the
  // front PaymentHandler view here.
  ViewStack* view_stack = dialog_view()->view_stack_for_testing();

  EXPECT_TRUE(IsViewVisible(DialogViewID::CANCEL_BUTTON, view_stack->top()));
  EXPECT_FALSE(IsViewVisible(DialogViewID::BACK_BUTTON, view_stack->top()));
  EXPECT_TRUE(IsViewVisible(DialogViewID::SHEET_TITLE, view_stack->top()));
  EXPECT_TRUE(
      IsViewVisible(DialogViewID::PAYMENT_APP_HEADER_ICON, view_stack->top()));
  EXPECT_TRUE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET,
                            view_stack->top()));

  // Only the origin is shown and is marked as the title. For this test, the
  // origin can be derived from the method name.
  ASSERT_TRUE(base::StartsWith(method_name, "https://"));
  EXPECT_EQ(base::ASCIIToUTF16(method_name.substr(8)),
            GetLabelText(DialogViewID::SHEET_TITLE, view_stack->top()));
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerHeaderViewUITest, HeaderWithoutIcon) {
  std::string method_name;
  InstallPaymentAppWithoutIcon("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest. Since the Payment App has no icon this will show
  // the browser sheet first, and we have to manually select the payment app to
  // continue.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Select the installed payment app.
  OpenPaymentMethodScreen();
  ResetEventWaiter(DialogEvent::BACK_NAVIGATION);
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  ASSERT_TRUE(list_view);
  EXPECT_EQ(1u, list_view->children().size());
  ClickOnDialogViewAndWait(list_view->children()[0]);

  // The pay button should be enabled now.
  ASSERT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON);

  // The payment app has no icon, so it should not be displayed on the header.
  EXPECT_FALSE(IsViewVisible(DialogViewID::PAYMENT_APP_HEADER_ICON));
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerHeaderViewUITest, CloseButtonPressed) {
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_handler_sw.js", &b_method_name);

  // Trigger PaymentRequest. Since there are two payment apps this will show
  // the browser sheet first, and we have to manually select the payment app to
  // continue.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "launchWithoutWaitForResponseWithMethods([{supportedMethods:$1}"
              ", {supportedMethods:$2}])",
              a_method_name, b_method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Select the installed payment app.
  OpenPaymentMethodScreen();
  ResetEventWaiter(DialogEvent::BACK_NAVIGATION);
  views::View* list_view = dialog_view()->GetViewByID(
      static_cast<int>(DialogViewID::PAYMENT_METHOD_SHEET_LIST_VIEW));
  ASSERT_TRUE(list_view);
  EXPECT_EQ(2u, list_view->children().size());
  ClickOnDialogViewAndWait(list_view->children()[0]);

  // The pay button should be enabled now.
  ASSERT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON);

  // The cancel button is shown and closes the dialog.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

// Test that the header and dialog heights are consistent with when there is no
// title.
// Flakily failing: https://crbug.com/1430351
IN_PROC_BROWSER_TEST_F(PaymentHandlerHeaderViewUITest,
                       DISABLED_ConsistentHeaderHeight) {
  // Install a payment app that will open a window.
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Trigger PaymentRequest.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  ViewStack* view_stack = dialog_view()->view_stack_for_testing();
  int header_height_with_title =
      view_stack->top()
          ->GetViewByID(static_cast<int>(DialogViewID::PAYMENT_APP_HEADER))
          ->height();
  int dialog_height_with_title = view_stack->top()->height();

  // Close the dialog.
  ClickOnCancel();

  // Now trigger PaymentRequest for a PaymentHandler with a window that has no
  // title.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED});
  ASSERT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("launchWithoutWaitForResponse($1, "
                                   "'payment_handler_window_no_title.html')",
                                   method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // Expect the dialog and header height with a title to be the same as before.
  view_stack = dialog_view()->view_stack_for_testing();
  EXPECT_EQ(dialog_height_with_title, view_stack->top()->height());
  EXPECT_EQ(
      header_height_with_title,
      view_stack->top()
          ->GetViewByID(static_cast<int>(DialogViewID::PAYMENT_APP_HEADER))
          ->height());

  ClickOnCancel();
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerHeaderViewUITest, LargeIcon) {
  // Install a payment app with a large icon that will be sized down at render.
  std::string method_name = PaymentAppInstallUtil::InstallPaymentApp(
      *GetActiveWebContents(), *https_server(), "a.com",
      "/payment_handler_sw.js",
      PaymentAppInstallUtil::IconInstall::kWithLargeIcon);

  // Trigger PaymentRequest, and wait until the PaymentHandler has loaded a
  // web-contents that has set a title.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::DIALOG_OPENED,
                               DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::PAYMENT_HANDLER_TITLE_SET});
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  ASSERT_TRUE(WaitForObservedEvent());

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a SHEET_TITLE, we have to explicitly test the
  // front PaymentHandler view here.
  ViewStack* view_stack = dialog_view()->view_stack_for_testing();
  EXPECT_TRUE(
      IsViewVisible(DialogViewID::PAYMENT_APP_HEADER_ICON, view_stack->top()));
  EXPECT_EQ(
      gfx::Size(
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight,
          IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight),
      static_cast<views::ImageView*>(
          GetChildByDialogViewID(view_stack,
                                 DialogViewID::PAYMENT_APP_HEADER_ICON))
          ->GetImageBounds()
          .size());
}

}  // namespace
}  // namespace payments
