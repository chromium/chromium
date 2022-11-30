// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_response.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests that two payment response objects are not equal if their property
// values differ or one is missing a value present in the other, and equal
// otherwise. Doesn't test all properties of child objects, relying instead on
// their respective tests.
TEST(PaymentRequestTest, PaymentResponseEquality) {
  PaymentResponse response1;
  PaymentResponse response2;
  EXPECT_EQ(response1, response2);

  response1.method_name = "Visa";
  EXPECT_NE(response1, response2);
  response2.method_name = "Mastercard";
  EXPECT_NE(response1, response2);
  response2.method_name = "Visa";
  EXPECT_EQ(response1, response2);

  std::string stringified_card_response1 =
      "{ \"cardNumber\": \"4111111111111111\", \"cardSecurityCode\": \"111\", "
      "\"cardholderName\": \"John Doe\", \"expiryMonth\": \"12\", "
      "\"expiryYear\": \"2020\" }";
  std::string stringified_card_response2 =
      "{ \"cardNumber\": \"4111111111111111\", \"cardSecurityCode\": \"333\", "
      "\"cardholderName\": \"John Doe\", \"expiryMonth\": \"12\", "
      "\"expiryYear\": \"2020\" }";
  response1.details = stringified_card_response1;
  EXPECT_NE(response1, response2);
  response2.details = stringified_card_response2;
  EXPECT_NE(response1, response2);
  response2.details = stringified_card_response1;
  EXPECT_EQ(response1, response2);
}

}  // namespace payments
