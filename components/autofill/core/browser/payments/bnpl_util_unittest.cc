// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_util.h"

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

struct EligibleBnplIssuerParams {
  BnplIssuer::IssuerId issuer_id;
  int expected_issuer_selection_text_id;
};

class BnplUtilEligibleTest
    : public testing::Test,
      public testing::WithParamInterface<EligibleBnplIssuerParams> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    BnplUtilEligibleTest,
    testing::Values(
        EligibleBnplIssuerParams{
            BnplIssuer::IssuerId::kBnplAffirm,
            IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY},
        EligibleBnplIssuerParams{
            BnplIssuer::IssuerId::kBnplAfterpay,
            IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY},
        EligibleBnplIssuerParams{
            BnplIssuer::IssuerId::kBnplKlarna,
            IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_KLARNA},
        EligibleBnplIssuerParams{
            BnplIssuer::IssuerId::kBnplZip,
            IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_ZIP}));

TEST_P(BnplUtilEligibleTest, GetBnplIssuerSelectionOptionText) {
  const EligibleBnplIssuerParams& params = GetParam();
  std::vector<BnplIssuerContext> issuer_contexts = {
      BnplIssuerContext(test::GetTestLinkedBnplIssuer(),
                        BnplIssuerEligibilityForPage::kIsEligible),
      BnplIssuerContext(
          test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
          BnplIssuerEligibilityForPage::kIsEligible),
      BnplIssuerContext(
          test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAfterpay),
          BnplIssuerEligibilityForPage::kIsEligible),
      BnplIssuerContext(
          test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna),
          BnplIssuerEligibilityForPage::kIsEligible)};

  EXPECT_EQ(
      GetBnplIssuerSelectionOptionText(params.issuer_id, "en-US",
                                       issuer_contexts),
      l10n_util::GetStringUTF16(params.expected_issuer_selection_text_id));
}

TEST(BnplUtilTest,
     GetBnplIssuerSelectionOptionText_NotEligibleIssuerDoesNotSupportMerchant) {
  std::vector<BnplIssuerContext> issuer_contexts = {BnplIssuerContext(
      test::GetTestLinkedBnplIssuer(),
      BnplIssuerEligibilityForPage::kNotEligibleIssuerDoesNotSupportMerchant)};

  EXPECT_EQ(
      GetBnplIssuerSelectionOptionText(BnplIssuer::IssuerId::kBnplAffirm,
                                       "en-US", issuer_contexts),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_NOT_SUPPORTED_BY_MERCHANT));
}

TEST(BnplUtilTest,
     GetBnplIssuerSelectionOptionText_NotEligibleCheckoutAmountTooLow) {
  std::vector<BnplIssuerContext> issuer_contexts = {BnplIssuerContext(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
      BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow)};

  EXPECT_EQ(
      GetBnplIssuerSelectionOptionText(BnplIssuer::IssuerId::kBnplZip, "en-US",
                                       issuer_contexts),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          u"$50.00"));
}

TEST(BnplUtilTest,
     GetBnplIssuerSelectionOptionText_NotEligibleCheckoutAmountTooHigh) {
  std::vector<BnplIssuerContext> issuer_contexts = {BnplIssuerContext(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAfterpay),
      BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh)};

  EXPECT_EQ(
      GetBnplIssuerSelectionOptionText(BnplIssuer::IssuerId::kBnplAfterpay,
                                       "en-US", issuer_contexts),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_HIGH,
          u"$200.00"));
}

TEST(BnplUtilTest,
     GetBnplIssuerSelectionOptionText_NotEligible_LargeNumberFormatting) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range(
      /*currency=*/"USD", /*price_lower_bound=*/50'000'000,
      /*price_upper_bound=*/30'000'000'000);
  issuer.set_eligible_price_ranges({price_range});

  std::vector<BnplIssuerContext> issuer_contexts = {BnplIssuerContext(
      issuer, BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh)};

  EXPECT_EQ(
      GetBnplIssuerSelectionOptionText(BnplIssuer::IssuerId::kBnplAffirm,
                                       "en-US", issuer_contexts),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_HIGH,
          u"$30,000.00"));
}

TEST(BnplUtilTest,
     GetBnplIssuerSelectionOptionText_NotEligible_DecimalFormatting) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range = BnplIssuer::EligiblePriceRange(
      /*currency=*/"USD", /*price_lower_bound=*/49'491'234,
      /*price_upper_bound=*/30'000'000'000);
  issuer.set_eligible_price_ranges({price_range});

  std::vector<BnplIssuerContext> issuer_contexts = {BnplIssuerContext(
      issuer, BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow)};

  // Check that `$49.491234` truncates to `$49.49`.
  EXPECT_EQ(
      GetBnplIssuerSelectionOptionText(BnplIssuer::IssuerId::kBnplAffirm,
                                       "en-US", issuer_contexts),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          u"$49.49"));
}

TEST(BnplUtilTest,
     GetBnplIssuerSelectionOptionText_NotEligible_DecimalRounding) {
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  BnplIssuer::EligiblePriceRange price_range = BnplIssuer::EligiblePriceRange(
      /*currency=*/"USD", /*price_lower_bound=*/99'999'999,
      /*price_upper_bound=*/30'000'000'000);
  issuer.set_eligible_price_ranges({price_range});

  std::vector<BnplIssuerContext> issuer_contexts = {BnplIssuerContext(
      issuer, BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow)};

  // Check that `$99.9999` rounds up to `$100.00`.
  EXPECT_EQ(
      GetBnplIssuerSelectionOptionText(BnplIssuer::IssuerId::kBnplAffirm,
                                       "en-US", issuer_contexts),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          u"$100.00"));
}

}  // namespace autofill::payments
