// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentHandlerWindowSizeTest : public PaymentRequestBrowserTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  PaymentHandlerWindowSizeTest(const PaymentHandlerWindowSizeTest&) = delete;
  PaymentHandlerWindowSizeTest& operator=(const PaymentHandlerWindowSizeTest&) =
      delete;

 protected:
  PaymentHandlerWindowSizeTest()
      : payment_handler_pop_up_size_window_enabled_(GetParam()),
        expected_payment_request_dialog_size_(
            gfx::Size(kDialogMinWidth, kDialogHeight)) {
    if (payment_handler_pop_up_size_window_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPaymentHandlerPopUpSizeWindow);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kPaymentHandlerPopUpSizeWindow);
    }
  }

  ~PaymentHandlerWindowSizeTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler.html");
  }

  gfx::Size DialogViewSize() { return dialog_view()->CalculatePreferredSize(); }

  const bool payment_handler_pop_up_size_window_enabled_;
  const gfx::Size expected_payment_request_dialog_size_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerWindowSizeTest, ValidateDialogSize) {
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

  gfx::Size expected_payment_handler_dialog_size;
  if (payment_handler_pop_up_size_window_enabled_) {
    // Adjust the expected PH window height based on the browser content height.
    int browser_window_content_height =
        browser()->window()->GetContentsSize().height();
    expected_payment_handler_dialog_size = gfx::Size(
        kPreferredPaymentHandlerDialogWidth,
        std::max(kDialogHeight, std::min(kPreferredPaymentHandlerDialogHeight,
                                         browser_window_content_height)));
  } else {
    expected_payment_handler_dialog_size =
        gfx::Size(kDialogMinWidth, kDialogHeight);
  }

  // Click on Pay and check dialog size when payment handler view is shown.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  EXPECT_EQ(expected_payment_handler_dialog_size, DialogViewSize());

  // Check that dialog size resets after back navigation from payment handler
  // window.
  ClickOnBackArrow();
  EXPECT_EQ(expected_payment_request_dialog_size_, DialogViewSize());
}

INSTANTIATE_TEST_SUITE_P(All, PaymentHandlerWindowSizeTest, testing::Bool());

}  // namespace payments
