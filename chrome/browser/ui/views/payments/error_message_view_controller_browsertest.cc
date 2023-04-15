// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

using PaymentRequestErrorMessageTest = PaymentRequestBrowserTestBase;

// Testing the use of the complete('fail') JS API and the error message.
IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageTest, CompleteFail) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_fail_complete_test.html");

  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");

  // We are ready to pay.
  ASSERT_TRUE(IsPayButtonEnabled());

  // Once "Pay" is clicked, the page will call complete('fail') and the error
  // message should be shown.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  EXPECT_FALSE(dialog_view()->throbber_overlay_for_testing()->GetVisible());

  // The user can only close the dialog at this point.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageTest,
                       EnterKeyClosesErrorDialog) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_fail_complete_test.html");

  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");

  // We are ready to pay.
  ASSERT_TRUE(IsPayButtonEnabled());

  // Once "Pay" is clicked, the page will call complete('fail') and the error
  // message should be shown.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // Trigger the 'Enter' accelerator, which should be present and mapped to
  // close the dialog.
  views::View* error_sheet =
      dialog_view()->GetViewByID(static_cast<int>(DialogViewID::ERROR_SHEET));
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  EXPECT_TRUE(error_sheet->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  ASSERT_TRUE(WaitForObservedEvent());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestErrorMessageTest,
                       ContentViewNotScrollable) {
  std::string payment_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &payment_method_name);

  NavigateTo("/payment_request_fail_complete_test.html");

  InvokePaymentRequestUIWithJs("buyWithMethods([{supportedMethods:'" +
                               payment_method_name + "'}]);");

  // We are ready to pay.
  ASSERT_TRUE(IsPayButtonEnabled());

  // Once "Pay" is clicked, the page will call complete('fail') and the error
  // message should be shown.
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::ERROR_MESSAGE_SHOWN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());

  // We always push the initial browser sheet to the stack, even if it isn't
  // shown. Since it also defines a CONTENT_VIEW, we have to explicitly test the
  // front PaymentHandler view here.
  views::View* top_view = dialog_view()->view_stack_for_testing()->top();

  views::View* sheet_view =
      GetChildByDialogViewID(top_view, DialogViewID::ERROR_SHEET);
  // The content view should be within the sheet view.
  EXPECT_NE(nullptr,
            GetChildByDialogViewID(sheet_view, DialogViewID::CONTENT_VIEW));

  // There should be no scroll view.
  EXPECT_EQ(nullptr, GetChildByDialogViewID(
                         top_view, DialogViewID::PAYMENT_SHEET_SCROLL_VIEW));
}

}  // namespace payments
