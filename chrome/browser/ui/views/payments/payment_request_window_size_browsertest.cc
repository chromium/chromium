// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/rect.h"

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
      PaymentRequestDialogView::WindowSizeCheckRejectionReason::kRejectedAtShow,
      1);
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
      PaymentRequestDialogView::WindowSizeCheckRejectionReason::
          kRejectedAtResize,
      1);
}

}  // namespace payments
