// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestBlobUrlTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestBlobUrlTest() {}
};

IN_PROC_BROWSER_TEST_F(PaymentRequestBlobUrlTest, Rejected) {
  NavigateTo("/payment_request_blob_url_test.html");

  // Trigger the Blob URL load, and wait for it to finish.
  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "(function() { document.getElementById('buy').click(); })();"));
  WaitForLoadStop(GetActiveWebContents());

  // Trigger the PaymentRequest, which should be rejected.
  ASSERT_EQ(
      "NotSupportedError: Only localhost, file://, and cryptographic scheme "
      "origins allowed.",
      content::EvalJs(GetActiveWebContents(), "triggerPaymentRequest();"));
}

}  // namespace payments
