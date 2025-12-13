// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {
namespace {

using IssuerId = autofill::BnplIssuer::IssuerId;
using ::testing::_;
using ::testing::Return;
using ::testing::Test;

struct EligibleBnplIssuerParams {
  BnplIssuer::IssuerId issuer_id;
  int expected_issuer_selection_text_id;
};

class BnplUtilEligibleTest
    : public Test,
      public testing::WithParamInterface<EligibleBnplIssuerParams> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    BnplUtilEligibleTest,
#if BUILDFLAG(IS_ANDROID)
    testing::Values(
        EligibleBnplIssuerParams{
            BnplIssuer::IssuerId::kBnplAffirm,
            IDS_AUTOFILL_BNPL_ISSUER_SELECTION_TEXT_AFFIRM_BOTTOM_SHEET},
        EligibleBnplIssuerParams{
            BnplIssuer::IssuerId::kBnplKlarna,
            IDS_AUTOFILL_BNPL_ISSUER_SELECTION_TEXT_KLARNA_BOTTOM_SHEET},
        EligibleBnplIssuerParams{
            BnplIssuer::IssuerId::kBnplZip,
            IDS_AUTOFILL_BNPL_ISSUER_SELECTION_TEXT_ZIP_BOTTOM_SHEET}));
#else
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
#endif  // BUILDFLAG(IS_ANDROID)

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

class MockPaymentsDataManager : public TestPaymentsDataManager {
 public:
  using TestPaymentsDataManager::TestPaymentsDataManager;
  MOCK_METHOD(std::vector<BnplIssuer>, GetBnplIssuers, (), (const, override));
  MOCK_METHOD(bool,
              IsAutofillAmountExtractionAiTermsSeenPrefEnabled,
              (),
              (const, override));
};

class BnplUtilTest : public Test, public WithTestAutofillClientDriverManager<> {
 public:
  const std::string kCurrency = "USD";

  BnplUtilTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillEnableAmountExtraction,
         features::kAutofillEnableBuyNowPayLaterSyncing,
         features::kAutofillEnableBuyNowPayLater,
         features::kAutofillEnableAiBasedAmountExtraction},
        /*disabled_features=*/{});
  }

 protected:
  void SetUp() override {
    InitAutofillClient();

    autofill_client().GetPersonalDataManager().set_payments_data_manager(
        std::make_unique<MockPaymentsDataManager>());

    ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                autofill_client().GetAutofillOptimizationGuideDecider()),
            IsUrlEligibleForBnplIssuer)
        .WillByDefault(Return(true));

    ON_CALL(
        static_cast<MockPaymentsDataManager&>(
            autofill_client().GetPersonalDataManager().payments_data_manager()),
        GetBnplIssuers)
        .WillByDefault(
            Return(std::vector<BnplIssuer>{test::GetTestLinkedBnplIssuer()}));
  }

  // Sets up the PersonalDataManager with an unlinked bnpl issuer.
  void SetUpUnlinkedBnplIssuer(uint64_t price_lower_bound_in_micros,
                               uint64_t price_higher_bound_in_micros,
                               IssuerId issuer_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(kCurrency, price_lower_bound_in_micros,
                                       price_higher_bound_in_micros);
    test_api(autofill_client().GetPersonalDataManager().payments_data_manager())
        .AddBnplIssuer(BnplIssuer(std::nullopt, issuer_id,
                                  std::move(eligible_price_ranges)));
  }

  // Sets up the PersonalDataManager with a linked bnpl issuer.
  void SetUpLinkedBnplIssuer(uint64_t price_lower_bound_in_micros,
                             uint64_t price_higher_bound_in_micros,
                             IssuerId issuer_id,
                             const int64_t instrument_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(kCurrency, price_lower_bound_in_micros,
                                       price_higher_bound_in_micros);

    test_api(autofill_client().GetPersonalDataManager().payments_data_manager())
        .AddBnplIssuer(BnplIssuer(instrument_id, issuer_id,
                                  std::move(eligible_price_ranges)));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BnplUtilTest, IsEligible_ReturnsTrueForEligibleIssuers) {
  BnplIssuerContext issuer_context(test::GetTestLinkedBnplIssuer(),
                                   BnplIssuerEligibilityForPage::kIsEligible);
  EXPECT_TRUE(issuer_context.IsEligible());

  BnplIssuerContext issuer_context_temporarily_eligible(
      test::GetTestLinkedBnplIssuer(),
      BnplIssuerEligibilityForPage::
          kTemporarilyEligibleCheckoutAmountNotYetKnown);
  EXPECT_TRUE(issuer_context_temporarily_eligible.IsEligible());
}

TEST_F(BnplUtilTest, IsEligible_ReturnsFalseForIneligibleIssuers) {
  BnplIssuerContext issuer_context_merchant(
      test::GetTestLinkedBnplIssuer(),
      BnplIssuerEligibilityForPage::kNotEligibleIssuerDoesNotSupportMerchant);
  EXPECT_FALSE(issuer_context_merchant.IsEligible());

  BnplIssuerContext issuer_context_low_amount(
      test::GetTestLinkedBnplIssuer(),
      BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow);
  EXPECT_FALSE(issuer_context_low_amount.IsEligible());

  BnplIssuerContext issuer_context_high_amount(
      test::GetTestLinkedBnplIssuer(),
      BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh);
  EXPECT_FALSE(issuer_context_high_amount.IsEligible());
}

TEST_F(
    BnplUtilTest,
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

TEST_F(BnplUtilTest,
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

TEST_F(BnplUtilTest,
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

TEST_F(BnplUtilTest,
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

TEST_F(BnplUtilTest,
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

TEST_F(BnplUtilTest,
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

TEST_F(BnplUtilTest, GetBnplUiFooterText) {
  constexpr std::string_view kPaymentSettingsLinkText = "payment settings";
  size_t offset = 0;
  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION,
      base::UTF8ToUTF16(kPaymentSettingsLinkText), &offset);

  EXPECT_THAT(
      GetBnplUiFooterText(),
      testing::FieldsAre(
          text, gfx::Range(0, 0),
          gfx::Range(offset, offset + kPaymentSettingsLinkText.length()), _));
}

TEST_F(BnplUtilTest, GetBnplUiFooterTextForAi_AiTermsBold) {
  ON_CALL(
      static_cast<MockPaymentsDataManager&>(
          autofill_client().GetPersonalDataManager().payments_data_manager()),
      IsAutofillAmountExtractionAiTermsSeenPrefEnabled)
      .WillByDefault(Return(false));

  const std::u16string kExpectedFullFooterText =
      u"Google uses information from the checkout page and other relevant data "
      u"to offer these options. To hide pay later options, go to payment "
      u"settings";
  const std::u16string kExpectedBoldAiText =
      u"Google uses information from the checkout page and other relevant data "
      u"to offer these options.";
  const std::u16string kLinkText = u"payment settings";
  const size_t kLinkOffset = kExpectedFullFooterText.find(kLinkText);

  EXPECT_THAT(
      GetBnplUiFooterTextForAi(
          autofill_client().GetPersonalDataManager().payments_data_manager()),
      testing::FieldsAre(
          kExpectedFullFooterText, gfx::Range(0, kExpectedBoldAiText.length()),
          gfx::Range(kLinkOffset, kLinkOffset + kLinkText.length()), _));
}

TEST_F(BnplUtilTest, GetBnplUiFooterTextForAi_AiTermsNotBold) {
  ON_CALL(
      static_cast<MockPaymentsDataManager&>(
          autofill_client().GetPersonalDataManager().payments_data_manager()),
      IsAutofillAmountExtractionAiTermsSeenPrefEnabled)
      .WillByDefault(Return(true));

  const std::u16string kExpectedFullFooterText =
      u"Google uses information from the checkout page and other relevant data "
      u"to offer these options. To hide pay later options, go to payment "
      u"settings";
  const std::u16string kLinkText = u"payment settings";
  const size_t kLinkOffset = kExpectedFullFooterText.find(kLinkText);

  EXPECT_THAT(
      GetBnplUiFooterTextForAi(
          autofill_client().GetPersonalDataManager().payments_data_manager()),
      testing::FieldsAre(
          kExpectedFullFooterText, gfx::Range(0, 0),
          gfx::Range(kLinkOffset, kLinkOffset + kLinkText.length()), _));
}

// Verify that if the triggering field is CVC, the BNPL option should not be
// appended.
TEST_F(BnplUtilTest, ShouldAppendBnplSuggestion_IsCvcField) {
  EXPECT_FALSE(ShouldAppendBnplSuggestion(autofill_client(),
                                          /*is_card_number_field_empty=*/true,
                                          CREDIT_CARD_VERIFICATION_CODE));
}

// Verify that if there was some content filled in the card number field, the
// BNPL option should not be appended.
TEST_F(BnplUtilTest, ShouldAppendBnplSuggestion_CardNumberFilled) {
  EXPECT_FALSE(ShouldAppendBnplSuggestion(autofill_client(),
                                          /*is_card_number_field_empty=*/false,
                                          CREDIT_CARD_NUMBER));
}

// Verify that if this profile is not eligible for BNPL, the BNPL option should
// not be appended.
TEST_F(BnplUtilTest, ShouldAppendBnplSuggestion_BnplNotEligible) {
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));

  EXPECT_FALSE(ShouldAppendBnplSuggestion(autofill_client(),
                                          /*is_card_number_field_empty=*/true,
                                          CREDIT_CARD_NUMBER));
}

// Verify that if the feature flag `kAutofillEnableAiBasedAmountExtraction` is
// disabled, the BNPL option should not be appended.
TEST_F(BnplUtilTest, ShouldAppendBnplSuggestion_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{features::kAutofillEnableAiBasedAmountExtraction});

  EXPECT_FALSE(ShouldAppendBnplSuggestion(autofill_client(),
                                          /*is_card_number_field_empty=*/true,
                                          CREDIT_CARD_NUMBER));
}

// Verify when the triggering field is not CVC, the suggestion is not empty,
// this profile is eligible for BNPL on the non-Android platform, the BNPL
// option should be appended.
TEST_F(BnplUtilTest, ShouldAppendBnplSuggestion_AllConditionsMet) {
  FieldType trigger_field = CREDIT_CARD_NUMBER;
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    EXPECT_FALSE(ShouldAppendBnplSuggestion(
        autofill_client(), /*is_card_number_field_empty=*/true, trigger_field));
  } else {
    EXPECT_TRUE(ShouldAppendBnplSuggestion(
        autofill_client(), /*is_card_number_field_empty=*/true, trigger_field));
  }
}

// Tests that `IsEligibleForBnpl()` returns false if the client does not have
// an `AutofillOptimizationGuideDecider` assigned.
TEST_F(BnplUtilTest, IsEligibleForBnpl_NoAutofillOptimizationGuideDecider) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm, /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  autofill_client().ResetAutofillOptimizationGuideDecider();
  EXPECT_FALSE(IsEligibleForBnpl(autofill_client()));
}

// Tests that `IsEligibleForBnpl()` returns false if the client is in an
// off-the-record (incognito) session.
TEST_F(BnplUtilTest, IsEligibleForBnpl_OffTheRecord) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm, /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  EXPECT_TRUE(IsEligibleForBnpl(autofill_client()));
  autofill_client().set_is_off_the_record(true);
  EXPECT_FALSE(IsEligibleForBnpl(autofill_client()));
}

// Tests that `IsEligibleForBnpl()` returns false if the current visiting
// url is not in the allowlist.
TEST_F(BnplUtilTest, IsEligibleForBnpl_UrlNotSupported) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm, /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));
  EXPECT_FALSE(IsEligibleForBnpl(autofill_client()));
}

// Tests that when the current visiting url is only supported by one of the
// BNPL issuers, `IsEligibleForBnpl()` returns true.
TEST_F(BnplUtilTest, IsEligibleForBnpl_UrlSupportedByOneIssuer) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm, /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplAffirm, _))
      .WillByDefault(Return(true));
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplZip, _))
      .WillByDefault(Return(false));
  EXPECT_TRUE(IsEligibleForBnpl(autofill_client()));
}

}  // namespace
}  // namespace autofill::payments
