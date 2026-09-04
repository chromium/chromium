// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_test_api.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

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

#if BUILDFLAG(IS_OZONE)
    // Wayland does not support resizing the window bounds programmatically,
    // which means these tests cannot function.
    if (ui::OzonePlatform::RunningOnWaylandForTest()) {
      GTEST_SKIP();
    }
#endif

    NavigateTo("/payment_handler.html");
  }

  gfx::Size DialogViewSize() {
    return test_api(dialog_view()).CalculatePreferredSize({});
  }

  const gfx::Size expected_payment_request_dialog_size_;

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPaymentRequestMandatoryPaymentAppUi};
};

// TODO(crbug.com/557001558): enable the flaky test on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ValidateDialogSize DISABLED_ValidateDialogSize
#else
#define MAYBE_ValidateDialogSize ValidateDialogSize
#endif
IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest, MAYBE_ValidateDialogSize) {
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
      BrowserWindow::FromBrowser(browser())->GetContentsSize().height();
  gfx::Size expected_payment_handler_dialog_size = gfx::Size(
      kPreferredPaymentHandlerDialogWidth,
      std::max(kDialogHeight, std::min(kPreferredPaymentHandlerDialogHeight,
                                       browser_window_content_height)));

  // Click on Pay and check dialog size when payment handler view is shown.
  EXPECT_TRUE(IsPayButtonEnabled());
  ResetEventWaiterForSequence({DialogEvent::LOADING_VIEW_SHOWN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED,
                               DialogEvent::LOADING_VIEW_HIDDEN});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  EXPECT_EQ(expected_payment_handler_dialog_size, DialogViewSize());

  // The test flakily hangs if we don't close the payment handler dialog.
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);
  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest, ResizeToPreferredHeight) {
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  // When browser content height >= 500, dialog height is capped at
  // kPreferredPaymentHandlerDialogHeight (500).
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 800));

  test_api(dialog_view()).ResizeToPaymentHandlerSize();

  EXPECT_EQ(gfx::Size(kPreferredPaymentHandlerDialogWidth,
                      kPreferredPaymentHandlerDialogHeight),
            DialogViewSize());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest,
                       ResizeToBrowserContentHeight) {
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  // Set window height to 600, which produces expected dialog height equals to
  // actual browser content height.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 600));

  test_api(dialog_view()).ResizeToPaymentHandlerSize();

  int actual_content_height =
      BrowserWindow::FromBrowser(browser())->GetContentsSize().height();
  EXPECT_EQ(
      gfx::Size(kPreferredPaymentHandlerDialogWidth, actual_content_height),
      DialogViewSize());
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerWindowSizeTest,
                       ResizeToMinimumDialogHeight) {
  std::string payment_method;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &payment_method);
  ResetEventWaiterForDialogOpened();
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true}, $1)",
          payment_method),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_TRUE(WaitForObservedEvent());
  // Set window height to 350 so browser content height < kDialogHeight (450).
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 350));

  test_api(dialog_view()).ResizeToPaymentHandlerSize();

  EXPECT_EQ(gfx::Size(kPreferredPaymentHandlerDialogWidth, kDialogHeight),
            DialogViewSize());
}

}  // namespace payments
