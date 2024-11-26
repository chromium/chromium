// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bnpl_issuer.h"

#include <string>

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/payment_instrument.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Test for getting and setting the issuer id for the BNPL issuer data model.
TEST(BnplIssuerTest, GetAndSetIssuerId) {
  BnplIssuer issuer = test::GetTestBnplIssuer();
  issuer.set_issuer_id("new_issuer");
  EXPECT_EQ(issuer.issuer_id(), "new_issuer");
}

// Test for getting and setting the payment instrument for the BNPL issuer data
// model.
TEST(BnplIssuerTest, SetAndGetPaymentInstrument) {
  BnplIssuer issuer = test::GetTestBnplIssuer();
  PaymentInstrument new_payment_instrument = PaymentInstrument(
      /*instrument_id=*/123456789, /*nickname=*/u"new payment instrument",
      GURL::EmptyGURL(),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kCardNumber}));
  ASSERT_NE(issuer.payment_instrument(), new_payment_instrument);
  issuer.set_payment_instrument(new_payment_instrument);
  EXPECT_EQ(issuer.payment_instrument(), new_payment_instrument);
}

// Test for getting and setting the price lower bound for the BNPL issuer data
// model.
TEST(BnplIssuerTest, SetAndGetPriceLowerBound) {
  BnplIssuer issuer = test::GetTestBnplIssuer();
  uint64_t price_lower_bound = 20000000;
  ASSERT_NE(issuer.eligible_price_ranges()[0].price_lower_bound,
            price_lower_bound);
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", price_lower_bound,
      issuer.eligible_price_ranges()[0].price_upper_bound);
  issuer.set_eligible_price_ranges({price_range});
  EXPECT_EQ(issuer.eligible_price_ranges()[0].price_lower_bound,
            price_lower_bound);
}

// Test for getting and setting the price upper bound for the BNPL issuer data
// model.
TEST(BnplIssuerTest, SetAndGetPriceUpperBound) {
  BnplIssuer issuer = test::GetTestBnplIssuer();
  uint64_t price_upper_bound = 300000000;
  ASSERT_NE(issuer.eligible_price_ranges()[0].price_upper_bound,
            price_upper_bound);
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", issuer.eligible_price_ranges()[0].price_lower_bound,
      price_upper_bound);
  issuer.set_eligible_price_ranges({price_range});
  EXPECT_EQ(issuer.eligible_price_ranges()[0].price_upper_bound,
            price_upper_bound);
}

// Test for the equality operator for the BNPL issuer data model.
TEST(BnplIssuerTest, EqualityOperator) {
  BnplIssuer issuer1 = test::GetTestBnplIssuer();
  BnplIssuer issuer2 = test::GetTestBnplIssuer();

  EXPECT_TRUE(issuer1 == issuer2);

  issuer2.set_issuer_id("different_issuer");
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestBnplIssuer();
  issuer2.set_payment_instrument(PaymentInstrument(
      /*instrument_id=*/123456789, /*nickname=*/u"new payment instrument",
      /*display_icon_url=*/GURL::EmptyGURL(),
      /*supported_rails=*/
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kCardNumber})));
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/100000000,
      /*price_upper_bound=*/
      issuer2.eligible_price_ranges()[0].price_upper_bound);
  issuer2.set_eligible_price_ranges({price_range});
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestBnplIssuer();
  price_range.price_lower_bound =
      issuer2.eligible_price_ranges()[0].price_lower_bound;
  price_range.price_upper_bound = 10000000000;
  issuer2.set_eligible_price_ranges({price_range});
  EXPECT_FALSE(issuer1 == issuer2);
}

}  // namespace autofill
