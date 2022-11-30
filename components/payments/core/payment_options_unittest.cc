// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_options.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests that two payment options objects are not equal if their property values
// differ and equal otherwise.
TEST(PaymentRequestTest, PaymentOptionsEquality) {
  PaymentOptions options1;
  PaymentOptions options2;
  EXPECT_EQ(options1, options2);

  options1.request_payer_name = true;
  EXPECT_NE(options1, options2);
  options2.request_payer_name = true;
  EXPECT_EQ(options1, options2);

  options1.request_payer_email = true;
  EXPECT_NE(options1, options2);
  options2.request_payer_email = true;
  EXPECT_EQ(options1, options2);

  options1.request_payer_phone = true;
  EXPECT_NE(options1, options2);
  options2.request_payer_phone = true;
  EXPECT_EQ(options1, options2);

  options1.request_shipping = true;
  EXPECT_NE(options1, options2);
  options2.request_shipping = true;
  EXPECT_EQ(options1, options2);

  // payments::PaymentShippingType::SHIPPING is the default value for
  // shipping_type.
  options1.shipping_type = payments::PaymentShippingType::SHIPPING;
  EXPECT_EQ(options1, options2);
  options1.shipping_type = payments::PaymentShippingType::PICKUP;
  EXPECT_NE(options1, options2);
  options2.shipping_type = payments::PaymentShippingType::PICKUP;
  EXPECT_EQ(options1, options2);
}

}  // namespace payments
