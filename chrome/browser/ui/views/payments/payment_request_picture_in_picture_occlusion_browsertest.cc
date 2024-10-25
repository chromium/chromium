// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using PaymentRequestPictureInPictureOcclusionTest =
    PaymentRequestBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestPictureInPictureOcclusionTest,
                       OcclusionClosesDialog) {
  std::string a_method_name;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js",
                    &a_method_name);
  std::string b_method_name;
  InstallPaymentApp("b.com", "/payment_request_success_responder.js",
                    &b_method_name);
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUIWithJs(content::JsReplace(
      "buyWithMethods([{supportedMethods:$1}, {supportedMethods:$2}]);",
      a_method_name, b_method_name));
  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  static_cast<PictureInPictureOcclusionObserver*>(dialog_view())
      ->OnOcclusionStateChanged(/*occluded=*/true);

  ASSERT_TRUE(WaitForObservedEvent());
}

}  // namespace
}  // namespace payments
