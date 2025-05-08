// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"

#include <string>

#include "base/test/gtest_util.h"
#include "components/autofill/core/browser/data_model/payments/payment_instrument.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

// Test for getting and setting the issuer id for the BNPL issuer data model.
TEST(BnplIssuerTest, GetAndSetIssuerId) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  issuer.set_issuer_id(BnplIssuer::IssuerId::kBnplAffirm);
  EXPECT_EQ(issuer.issuer_id(), BnplIssuer::IssuerId::kBnplAffirm);
}

// Test for getting and setting the payment instrument for the BNPL issuer data
// model.
TEST(BnplIssuerTest, SetAndGetPaymentInstrument) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
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
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  uint64_t price_lower_bound = 20'000'000;
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
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  uint64_t price_upper_bound = 300'000'000;
  ASSERT_NE(issuer.eligible_price_ranges()[0].price_upper_bound,
            price_upper_bound);
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", issuer.eligible_price_ranges()[0].price_lower_bound,
      price_upper_bound);
  issuer.set_eligible_price_ranges({price_range});
  EXPECT_EQ(issuer.eligible_price_ranges()[0].price_upper_bound,
            price_upper_bound);
}

// Test for getting price range in given currency, and getting `std::nullopt`
// if the issuer doesn't have a price range in the currency.
TEST(BnplIssuerTest, GetEligiblePriceRangeForCurrency_WithRangeInUsd) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/50'000'000,
      /*price_upper_bound=*/200'000'000);
  issuer.set_eligible_price_ranges({price_range});
  const base::optional_ref<const BnplIssuer::EligiblePriceRange> usd_range =
      issuer.GetEligiblePriceRangeForCurrency("USD");
  ASSERT_TRUE(usd_range.has_value());
  EXPECT_EQ("USD", usd_range.value().currency);
  EXPECT_EQ(issuer.eligible_price_ranges()[0].price_upper_bound,
            usd_range.value().price_upper_bound);
  EXPECT_EQ(issuer.eligible_price_ranges()[0].price_lower_bound,
            usd_range.value().price_lower_bound);

  EXPECT_FALSE(issuer.GetEligiblePriceRangeForCurrency("GBP").has_value());
}

// Test that 'IsEligibleAmount' returns false if the given amount is not in
// supported range.
TEST(BnplIssuerTest, IsEligibleAmount_NotSupportedAmount) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/50'000'000,
      /*price_upper_bound=*/200'000'000);
  issuer.set_eligible_price_ranges({price_range});
  EXPECT_FALSE(issuer.IsEligibleAmount(/*amount_in_micros=*/30'000'000,
                                       /*currency=*/"USD"));
  EXPECT_FALSE(issuer.IsEligibleAmount(/*amount_in_micros=*/300'000'000,
                                       /*currency=*/"USD"));
}

// Test that 'IsEligibleAmount' returns false if the given currency is not in
// supported.
TEST(BnplIssuerTest, IsEligibleAmount_NotSupportedCurrency) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/50'000'000,
      /*price_upper_bound=*/200'000'000);
  issuer.set_eligible_price_ranges({price_range});
  EXPECT_FALSE(issuer.IsEligibleAmount(/*amount_in_micros=*/60'000'000,
                                       /*currency=*/"GBP"));
}

// Test that 'IsEligibleAmount' returns true for eligible currency and amount.
TEST(BnplIssuerTest, IsEligibleAmount) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/50'000'000,
      /*price_upper_bound=*/200'000'000);
  issuer.set_eligible_price_ranges({price_range});
  EXPECT_TRUE(issuer.IsEligibleAmount(/*amount_in_micros=*/60'000'000,
                                      /*currency=*/"USD"));
}

TEST(BnplIssuerTest, GetDisplayName) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  issuer.set_issuer_id(BnplIssuer::IssuerId::kBnplAffirm);
  EXPECT_EQ(issuer.GetDisplayName(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_AFFIRM));
  issuer.set_issuer_id(BnplIssuer::IssuerId::kBnplZip);
  EXPECT_EQ(issuer.GetDisplayName(),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_ZIP));
}

// Test for the equality operator for the BNPL issuer data model.
TEST(BnplIssuerTest, EqualityOperator) {
  BnplIssuer issuer1 = test::GetTestLinkedBnplIssuer();
  BnplIssuer issuer2 = test::GetTestLinkedBnplIssuer();

  EXPECT_TRUE(issuer1 == issuer2);

  issuer2.set_issuer_id(BnplIssuer::IssuerId::kBnplAfterpay);
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestLinkedBnplIssuer();
  issuer2.set_payment_instrument(PaymentInstrument(
      /*instrument_id=*/123456789, /*nickname=*/u"new payment instrument",
      /*display_icon_url=*/GURL::EmptyGURL(),
      /*supported_rails=*/
      DenseSet<PaymentInstrument::PaymentRail>(
          {PaymentInstrument::PaymentRail::kCardNumber})));
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/100'000'000,
      /*price_upper_bound=*/
      issuer2.eligible_price_ranges()[0].price_upper_bound);
  issuer2.set_eligible_price_ranges({price_range});
  EXPECT_FALSE(issuer1 == issuer2);

  issuer2 = test::GetTestLinkedBnplIssuer();
  price_range.price_lower_bound =
      issuer2.eligible_price_ranges()[0].price_lower_bound;
  price_range.price_upper_bound = 10'000'000'000;
  issuer2.set_eligible_price_ranges({price_range});
  EXPECT_FALSE(issuer1 == issuer2);
}

TEST(BnplIssuerTest, BnplIssuerIdToDisplayName) {
  EXPECT_EQ(BnplIssuerIdToDisplayName(BnplIssuer::IssuerId::kBnplAffirm),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_AFFIRM));
  EXPECT_EQ(BnplIssuerIdToDisplayName(BnplIssuer::IssuerId::kBnplZip),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_ZIP));
  EXPECT_EQ(BnplIssuerIdToDisplayName(BnplIssuer::IssuerId::kBnplAfterpay),
            l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_AFTER_PAY));
}

TEST(BnplIssuerTest, ConvertToBnplIssuerIdEnum) {
  EXPECT_EQ(ConvertToBnplIssuerIdEnum(kBnplAffirmIssuerId),
            BnplIssuer::IssuerId::kBnplAffirm);
  EXPECT_EQ(ConvertToBnplIssuerIdEnum(kBnplZipIssuerId),
            BnplIssuer::IssuerId::kBnplZip);
  EXPECT_EQ(ConvertToBnplIssuerIdEnum(kBnplAfterpayIssuerId),
            BnplIssuer::IssuerId::kBnplAfterpay);
}

TEST(BnplIssuerTest, ConvertToBnplIssuerIdString) {
  EXPECT_EQ(ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplAffirm),
            kBnplAffirmIssuerId);
  EXPECT_EQ(ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplZip),
            kBnplZipIssuerId);
  EXPECT_EQ(ConvertToBnplIssuerIdString(BnplIssuer::IssuerId::kBnplAfterpay),
            kBnplAfterpayIssuerId);
}

}  // namespace autofill
