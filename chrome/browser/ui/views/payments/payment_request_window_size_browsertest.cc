// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/numerics/safe_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/payments/core/features.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/widget_test_api.h"
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
  ResetEventWaiterForSequence(
      {DialogEvent::DIALOG_SIZE_CHECK_AFTER_BROWSER_RESIZE,
       DialogEvent::INTERNAL_ERROR});
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

// Regression test for crbug.com/572422506 where synchronous Picture-in-Picture
// window auto-enlarging caused a UAF via CheckIfDialogFitsInBrowserWindow.
IN_PROC_BROWSER_TEST_F(PaymentRequestWindowSizeTest,
                       RejectInAutoEnlargedPictureInPictureWindow) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &method_name);
  NavigateTo("/payment_request_no_shipping_test.html");

  // A newly created Document Picture-in-Picture window has a committed URL of
  // about:blank, which UrlUtil::IsOriginAllowedToUseWebPaymentApis() rejects.
  // Using document.open() followed by history.replaceState() updates the PiP
  // main frame's committed URL to the opener's HTTPS URL via a same-document
  // navigation without closing the PiP window.
  //
  // TODO(crbug.com/573056666): Either disallow Payment Request in PiP entirely,
  // or fix it so that this hack is not needed.
  ASSERT_EQ(true, content::EvalJs(
                      GetActiveWebContents(),
                      "documentPictureInPicture.requestWindow().then(w => { "
                      "w.document.open(); "
                      "w.document.close(); "
                      "w.history.replaceState(null, '', location.href); "
                      "return true; })"));

  content::WebContents* pip_web_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);

  views::Widget* pip_widget = views::Widget::GetTopLevelWidgetForNativeView(
      pip_web_contents->GetNativeView());
  ASSERT_NE(nullptr, pip_widget);

  // Size the PiP window to the minimum pre-widget size required by ShowDialog()
  // (1.05 * kDialogMinWidth x kDialogHeight, with a 0 top offset). When the
  // child dialog widget is shown, PipChildDialogObserverHelper synchronously
  // enlarges the PiP window to fit the unscaled dialog plus title bar padding
  // (without the 5% safety buffer), which then fails the post-widget size check
  // in CheckIfDialogFitsInBrowserWindow().
  const int width = base::ClampRound(kDialogMinWidth * 1.05f);
  const int height = base::ClampRound(kDialogHeight * 1.05f);
  views::AsyncWidgetRequestWaiter bounds_waiter(*pip_widget);
  pip_widget->SetBounds(gfx::Rect(0, 0, width, height));
  bounds_waiter.Wait();

  base::HistogramTester histograms;

  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::DIALOG_SIZE_CHECK_AFTER_BROWSER_RESIZE,
       DialogEvent::INTERNAL_ERROR});

  EXPECT_EQ(
      "NotSupportedError: Browser window is too small for Payment Request UI.",
      content::EvalJs(
          pip_web_contents,
          content::JsReplace(
              "const request = new PaymentRequest([{supportedMethods: $1}], "
              "{total: {label: 'Total', amount: {currency: 'USD', value: "
              "'1.00'}}}); "
              "request.show().catch(e => e.toString());",
              method_name)));

  ASSERT_TRUE(WaitForObservedEvent());

  histograms.ExpectUniqueSample(
      "PaymentRequest.WindowSizeCheckRejectionReason",
      JourneyLogger::WindowSizeCheckRejectionReason::kRejectedAtResize, 1);
}

}  // namespace payments
