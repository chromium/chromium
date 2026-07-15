// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/payments/core/features.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace payments {

class PaymentRequestWindowSizeTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestWindowSizeTest() {
    feature_list_.InitAndEnableFeature(
        features::kPaymentRequestRejectTooSmallWindows);
    SetBrowserWindowSizeCheckEnabled();
  }

#if BUILDFLAG(IS_OZONE)
  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

    // Wayland does not support resizing the window bounds programmatically,
    // which means these tests cannot function.
    if (ui::OzonePlatform::RunningOnWaylandForTest()) {
      GTEST_SKIP();
    }
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestWindowSizeTest,
                       RejectShowInTooSmallWindow) {
  base::HistogramTester histograms;

  // Set the browser window to be very small.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 100, 100));

  std::string method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &method_name);
  NavigateTo("/payment_request_no_shipping_test.html");

  // The PaymentRequest.show() call should be rejected immediately.
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::INTERNAL_ERROR});

  content::ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace("buyWithMethods([{supportedMethods:$1}]);",
                         method_name));

  ASSERT_TRUE(WaitForObservedEvent());

  histograms.ExpectUniqueSample(
      "PaymentRequest.WindowSizeCheckRejectionReason",
      JourneyLogger::WindowSizeCheckRejectionReason::kRejectedAtShow, 1);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestWindowSizeTest, AbortOnResizeToTooSmall) {
  base::HistogramTester histograms;

  // Start with a normal size window.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 600));

  std::string method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &method_name);
  NavigateTo("/payment_request_no_shipping_test.html");

  ResetEventWaiterForDialogOpened();
  content::ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace("buyWithMethods([{supportedMethods:$1}]);",
                         method_name));
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_NE(nullptr, dialog_view());

  // Resize the window to be too small.
  ResetEventWaiter(DialogEvent::INTERNAL_ERROR);
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 100, 100));

  // We need to wait for the throttle timer (100ms).
  ASSERT_TRUE(WaitForObservedEvent());

  histograms.ExpectUniqueSample(
      "PaymentRequest.WindowSizeCheckRejectionReason",
      JourneyLogger::WindowSizeCheckRejectionReason::kRejectedAtResize, 1);
}

// Regression test for a bug where the 5% 'safety buffer' was incorrectly
// applied to the offset of the dialog and not just the size. This could cause
// Payment Requests to be incorrectly rejected.
IN_PROC_BROWSER_TEST_F(PaymentRequestWindowSizeTest,
                       BufferShouldApplyToSizeNotOffset) {
  base::HistogramTester histograms;

  // Start with a large, safe window size to ensure the dialog opens.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 800));

  // Install two payment apps to prevent "skip the sheet". If only one app were
  // installed, the browser would automatically complete the payment and close
  // the dialog after it is shown, which would interfere with our ability to
  // verify that the dialog stays open after resize.
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &method_name);
  std::string method_name2;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &method_name2);
  NavigateTo("/payment_request_no_shipping_test.html");

  ResetEventWaiterForDialogOpened();
  content::ExecuteScriptAsync(
      GetActiveWebContents(),
      content::JsReplace(
          "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
          method_name, method_name2));
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_NE(nullptr, dialog_view());

  // Calculate the target size dynamically to avoid fragility due to platform
  // differences or future UI changes.
  views::Widget* browser_widget = views::Widget::GetWidgetForNativeWindow(
      GetActiveWebContents()->GetTopLevelNativeWindow());
  ASSERT_NE(nullptr, browser_widget);

  gfx::Rect dialog_bounds =
      dialog_view()->GetWidget()->GetWindowBoundsInScreen();
  gfx::Point origin_in_browser = views::View::ConvertPointFromScreen(
      browser_widget->GetRootView(), dialog_bounds.origin());

  int top_offset = origin_in_browser.y();
  int dialog_height = dialog_bounds.height();

  // The minimum required height is (top offset + scaled dialog height).
  int scaled_dialog_height = base::ClampRound(dialog_height * 1.05f);
  int required = top_offset + scaled_dialog_height;

  // If the offset were also scaled (which is incorrect), the required height
  // would be 1.05 * (top offset + dialog height).
  int required_scaled_with_top_offset =
      base::ClampRound((top_offset + dialog_height) * 1.05f);

  // Verify that scaling the top offset would have caused a rejection by
  // ensuring there is a gap between the calculations.
  ASSERT_LT(required, required_scaled_with_top_offset);

  // Resize the window to the minimum required height. This would have been
  // rejected if the top offset were incorrectly scaled.
  ResetEventWaiter(DialogEvent::DIALOG_SIZE_CHECK_AFTER_BROWSER_RESIZE);
  ui_test_utils::SetAndWaitForBounds(*browser(),
                                     gfx::Rect(0, 0, 800, required));
  ASSERT_TRUE(WaitForObservedEvent());

  // The dialog should still be open.
  EXPECT_NE(nullptr, dialog_view());
  histograms.ExpectBucketCount(
      "PaymentRequest.WindowSizeCheckRejectionReason",
      JourneyLogger::WindowSizeCheckRejectionReason::kRejectedAtResize, 0);
}

}  // namespace payments
