// Copyright 2017 The Chromium Authors. All rights reserved.
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

IN_PROC_BROWSER_TEST_F(PaymentRequestBlobUrlTest, ConnectionTerminated) {
  NavigateTo("/payment_request_blob_url_test.html");
  ResetEventWaiter(DialogEvent::NOT_SUPPORTED_ERROR);
  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "(function() { document.getElementById('buy').click(); })();"));
  WaitForObservedEvent();
  ExpectBodyContains({"Rejected: NotSupportedError"});
}

}  // namespace payments
