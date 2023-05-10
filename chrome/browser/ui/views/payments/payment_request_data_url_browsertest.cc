// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestDataUrlTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestDataUrlTest() {}
};

IN_PROC_BROWSER_TEST_F(PaymentRequestDataUrlTest, SecurityError) {
  NavigateTo(
      "data:text/html,<html><head><meta name=\"viewport\" "
      "content=\"width=device-width, initial-scale=1, "
      "maximum-scale=1\"></head><body><button id=\"buy\" onclick=\"try { "
      "(new PaymentRequest([{supportedMethods: ['basic-card']}], {total: "
      "{label: 'Total',  amount: {currency: 'USD', value: "
      "'1.00'}}})).show(); } catch(e) { "
      "document.getElementById('result').innerHTML = e; }\">Data URL "
      "Test</button><div id='result'></div></body></html>");

  // PaymentRequest should not be defined in non-secure context.
  ASSERT_EQ(false, content::EvalJs(GetActiveWebContents(),
                                   "'PaymentRequest' in window;"));

  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "(function() { document.getElementById('buy').click(); })();"));
  ExpectBodyContains({"PaymentRequest is not defined"});
}

}  // namespace payments
