// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace payments {
namespace {

// Looks for the "supportedMethods" URL and removes its port number.
std::string ClearPortNumber(const std::string& may_contain_method_url) {
  std::string before;
  std::string method;
  std::string after;
  GURL::Replacements port;
  port.ClearPort();
  return re2::RE2::FullMatch(
             may_contain_method_url,
             "(.*\"supportedMethods\":\")(https://.*)(\",\"total\".*)", &before,
             &method, &after)
             ? before + GURL(method).ReplaceComponents(port).spec() + after
             : may_contain_method_url;
}

enum class MethodIdentifier { kUrlBased, kBasicCard };

struct TestCase {
  TestCase(const std::string& init_test_code,
           const std::string& expected_output,
           MethodIdentifier method_identifier = MethodIdentifier::kUrlBased)
      : init_test_code(init_test_code),
        expected_output(expected_output),
        method_identifier(method_identifier) {}

  ~TestCase() {}

  const std::string init_test_code;
  const std::string expected_output;
  const MethodIdentifier method_identifier;
};

class PaymentHandlerChangePaymentMethodTest
    : public PaymentRequestBrowserTestBase,
      public testing::WithParamInterface<TestCase> {
 protected:
  PaymentHandlerChangePaymentMethodTest() {}
  ~PaymentHandlerChangePaymentMethodTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerChangePaymentMethodTest);
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerChangePaymentMethodTest, Test) {
  if (GetParam().method_identifier == MethodIdentifier::kBasicCard)
    SetSkipUiForForBasicCard();

  NavigateTo("/change_payment_method.html");

  if (GetParam().method_identifier == MethodIdentifier::kBasicCard) {
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       "basicCardMethodName();"));
  }
  std::string actual_output;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(), "install();", &actual_output));
  ASSERT_EQ(actual_output, "instruments.set(): Payment handler installed.");

  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                     GetParam().init_test_code));

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetActiveWebContents(), "outputChangePaymentMethodReturnValue(request);",
      &actual_output));

  // The test expectations are hard-coded, but the embedded test server changes
  // its port number in every test, e.g., https://a.com:34548.
  ASSERT_EQ(ClearPortNumber(actual_output), GetParam().expected_output)
      << "When executing " << GetParam().init_test_code;
}

INSTANTIATE_TEST_SUITE_P(
    NoMerchantResponse,
    PaymentHandlerChangePaymentMethodTest,
    testing::Values(
        TestCase("initTestNoHandler();",
                 "PaymentRequest.show(): changePaymentMethod() returned: null"),
        TestCase("initTestNoHandler();",
                 "PaymentRequest.show(): changePaymentMethod() returned: null",
                 MethodIdentifier::kBasicCard)));

INSTANTIATE_TEST_SUITE_P(
    ErrorCases,
    PaymentHandlerChangePaymentMethodTest,
    testing::Values(
        TestCase("initTestReject()",
                 "PaymentRequest.show() rejected with: Error for test"),
        TestCase("initTestReject()",
                 "PaymentRequest.show() rejected with: Error for test",
                 MethodIdentifier::kBasicCard),
        TestCase("initTestThrow()",
                 "PaymentRequest.show() rejected with: Error: Error for test"),
        TestCase("initTestThrow()",
                 "PaymentRequest.show() rejected with: Error: Error for test",
                 MethodIdentifier::kBasicCard)));

INSTANTIATE_TEST_SUITE_P(
    MerchantResponse,
    PaymentHandlerChangePaymentMethodTest,
    testing::Values(
        TestCase(
            "initTestDetails()",
            "PaymentRequest.show(): changePaymentMethod() returned: "
            "{\"error\":\"Error for test\","
            "\"modifiers\":[{\"data\":{\"soup\":\"potato\"},"
            "\"supportedMethods\":\"https://a.com/pay\","
            "\"total\":{\"amount\":{\"currency\":\"EUR\",\"value\":\"0.03\"},"
            "\"label\":\"\",\"pending\":false}}],"
            "\"paymentMethodErrors\":{\"country\":\"Unsupported country\"},"
            "\"total\":{\"currency\":\"GBP\",\"value\":\"0.02\"}}"),
        TestCase(
            "initTestDetails()",
            "PaymentRequest.show(): changePaymentMethod() returned: "
            "{\"error\":\"Error for test\","
            "\"modifiers\":[{\"data\":{\"soup\":\"potato\"},"
            "\"supportedMethods\":\"basic-card\","
            "\"total\":{\"amount\":{\"currency\":\"EUR\",\"value\":\"0.03\"},"
            "\"label\":\"\",\"pending\":false}}],"
            "\"paymentMethodErrors\":{\"country\":\"Unsupported country\"},"
            "\"total\":{\"currency\":\"GBP\",\"value\":\"0.02\"}}",
            MethodIdentifier::kBasicCard)));

}  // namespace
}  // namespace payments
