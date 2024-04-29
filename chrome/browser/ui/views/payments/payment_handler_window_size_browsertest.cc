// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentHandlerWindowSizeTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerWindowSizeTest(const PaymentHandlerWindowSizeTest&) = delete;
  PaymentHandlerWindowSizeTest& operator=(const PaymentHandlerWindowSizeTest&) =
      delete;

 protected:
  PaymentHandlerWindowSizeTest()
      : expected_payment_request_dialog_size_(
            gfx::Size(kDialogMinWidth, kDialogHeight)) {}

  ~PaymentHandlerWindowSizeTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler.html");
  }

  gfx::Size DialogViewSize() {
    return dialog_view()->CalculatePreferredSize({});
  }

  const gfx::Size expected_payment_request_dialog_size_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest, ValidateDialogSize) {
  // Add an autofill profile, so [Continue] button is enabled.
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);

  // Install a payment handler which opens a window.
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);

  // Invoke a payment request and then check the dialog size when payment sheet
  // is shown.
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_EQ(expected_payment_request_dialog_size_, DialogViewSize());

  // Adjust the expected PH window height based on the browser content height.
  int browser_window_content_height =
      browser()->window()->GetContentsSize().height();
  gfx::Size expected_payment_handler_dialog_size = gfx::Size(
      kPreferredPaymentHandlerDialogWidth,
      std::max(kDialogHeight, std::min(kPreferredPaymentHandlerDialogHeight,
                                       browser_window_content_height)));

  // Click on Pay and check dialog size when payment handler view is shown.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  EXPECT_EQ(expected_payment_handler_dialog_size, DialogViewSize());

  // The test flakily hangs if we don't close the payment handler dialog.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

}  // namespace payments
