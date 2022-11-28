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

}  // namespace payments
