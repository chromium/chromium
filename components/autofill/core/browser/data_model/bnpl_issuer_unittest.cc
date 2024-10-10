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
  ASSERT_NE(issuer.price_lower_bound(), 20);
  issuer.set_price_lower_bound(20);
  EXPECT_EQ(issuer.price_lower_bound(), 20);
}

// Test for getting and setting the price upper bound for the BNPL issuer data
// model.
TEST(BnplIssuerTest, SetAndGetPriceUpperBound) {
  BnplIssuer issuer = test::GetTestBnplIssuer();
  ASSERT_NE(issuer.price_upper_bound(), 300);
  issuer.set_price_upper_bound(300);
  EXPECT_EQ(issuer.price_upper_bound(), 300);
}

// Test for the strong ordering with the issuer id for the BNPL issuer data
// model.
TEST(BnplIssuerTest, StrongOrdering_IssuerId) {
  BnplIssuer issuer1 = test::GetTestBnplIssuer();
  BnplIssuer issuer2 = test::GetTestBnplIssuer();
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::equal);

  issuer2.set_issuer_id("zzz");
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::less);

  issuer2.set_issuer_id("aaa");
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::greater);
}

// Test for the strong ordering of the payment instrument for the BNPL issuer
// data model.
TEST(BnplIssuerTest, StrongOrdering_PaymentInstrument) {
  BnplIssuer issuer1 = test::GetTestBnplIssuer();
  BnplIssuer issuer2 = test::GetTestBnplIssuer();
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::equal);

  PaymentInstrument new_payment_instrument =
      PaymentInstrument(123456789, /*nickname=*/u"", GURL::EmptyGURL(),
                        DenseSet<PaymentInstrument::PaymentRail>(
                            {PaymentInstrument::PaymentRail::kCardNumber}));
  issuer2.set_payment_instrument(new_payment_instrument);
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::less);

  new_payment_instrument = PaymentInstrument(
      /*instrument_id=*/0000, /*nickname=*/u"", GURL::EmptyGURL(),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kCardNumber}));
  issuer2.set_payment_instrument(new_payment_instrument);
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::greater);
}

// Test for the strong ordering of the price lower bound for the BNPL issuer
// data model.
TEST(BnplIssuerTest, StrongOrdering_PriceLowerBound) {
  BnplIssuer issuer1 = test::GetTestBnplIssuer();
  BnplIssuer issuer2 = test::GetTestBnplIssuer();
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::equal);

  issuer2.set_price_lower_bound(10000000);
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::less);

  issuer2.set_price_lower_bound(0);
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::greater);
}

// Test for the strong ordering of the price upper bound for the BNPL issuer
// data model.
TEST(BnplIssuerTest, StrongOrdering_PriceUpperBound) {
  BnplIssuer issuer1 = test::GetTestBnplIssuer();
  BnplIssuer issuer2 = test::GetTestBnplIssuer();
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::equal);

  issuer2.set_price_upper_bound(10000000);
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::less);

  issuer2.set_price_upper_bound(0);
  EXPECT_EQ(issuer1 <=> issuer2, std::strong_ordering::greater);
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
      123456789, /*nickname=*/u"new payment instrument", GURL::EmptyGURL(),
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kCardNumber})));
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestBnplIssuer();
  issuer2.set_price_lower_bound(1000);
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestBnplIssuer();
  issuer2.set_price_upper_bound(10000);
  EXPECT_FALSE(issuer1 == issuer2);
}

}  // namespace autofill
