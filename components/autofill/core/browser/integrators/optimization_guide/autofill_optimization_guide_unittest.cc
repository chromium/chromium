// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide.h"

#include <algorithm>
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_benefit_test_api.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_test_api.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using test::CreateTestCreditCardFormData;
using test::CreateTestIbanFormData;
using ::testing::_;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

}  // namespace

class AutofillOptimizationGuideTest : public testing::Test {
 public:
  AutofillOptimizationGuideTest()
      : pref_service_(test::PrefServiceForTesting()),
        autofill_optimization_guide_(&decider()) {
    payments_data_manager_.SetPrefService(pref_service_.get());
    payments_data_manager_.SetSyncServiceForTest(&sync_service_);
  }

  CreditCard GetVcnEnrolledCard(
      std::string_view network = kVisaCard,
      CreditCard::VirtualCardEnrollmentType virtual_card_enrollment_type =
          CreditCard::VirtualCardEnrollmentType::kNetwork,
      std::string_view issuer_id = "",
      std::string_view benefit_source = "") {
    CreditCard card = test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
    test_api(card).set_network_for_card(network);
    card.set_virtual_card_enrollment_type(virtual_card_enrollment_type);
    test_api(card).set_issuer_id_for_card(issuer_id);
    card.set_benefit_source(benefit_source);
    return card;
  }

  void MockFlatRateCreditCardBenefitsBlockedDecisionForUrl(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision) {
    ON_CALL(
        decider(),
        CanApplyOptimization(
            Eq(url),
            Eq(optimization_guide::proto::
                   SHARED_CREDIT_CARD_FLAT_RATE_BENEFITS_BLOCKLIST),
            Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
        .WillByDefault(Return(decision));
  }

 protected:
  optimization_guide::MockOptimizationGuideDecider& decider() {
    return decider_;
  }
  AutofillOptimizationGuide& guide() { return autofill_optimization_guide_; }
  TestPaymentsDataManager& payments_data_manager() {
    return payments_data_manager_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<PrefService> pref_service_;
  syncer::TestSyncService sync_service_;
  optimization_guide::MockOptimizationGuideDecider decider_;
  TestPaymentsDataManager payments_data_manager_;
  AutofillOptimizationGuide autofill_optimization_guide_;
};

TEST_F(AutofillOptimizationGuideTest, EnsureIntegratorInitializedCorrectly) {
  EXPECT_TRUE(guide().GetOptimizationGuideKeyedServiceForTesting() ==
              &decider());
}

// Test that the `IBAN_AUTOFIL L_BLOCKED` optimization type is registered when
// we have seen an IBAN form.
TEST_F(AutofillOptimizationGuideTest, IbanFieldFound_IbanAutofillBlocked) {
  FormStructure form_structure{CreateTestIbanFormData()};
  test_api(form_structure).SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::IBAN_AUTOFILL_BLOCKED)));

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the corresponding optimization types are registered in the VCN
// merchant opt-out case when a credit card form is seen, and VCNs that have an
// associated optimization guide blocklist are present.
TEST_F(AutofillOptimizationGuideTest, CreditCardFormFound_VcnMerchantOptOut) {
  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard());
  payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCard(kDiscoverCard));
  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard(kMasterCard));

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""),
                                         /*log_manager=*/nullptr);

  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA,
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_DISCOVER,
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD)));

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the `VCN_MERCHANT_OPT_OUT_VISA` optimization type is not registered
// when we have seen a credit card form, but the network is not Visa.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_VcnMerchantOptOut_NotVisaNetwork) {
  payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCard(/*network=*/kAmericanExpressCard));

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""),
                                         /*log_manager=*/nullptr);

  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the `VCN_MERCHANT_OPT_OUT_VISA` optimization type is not registered
// when we have seen a credit card form, but the virtual card is an issuer-level
// enrollment
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_VcnMerchantOptOut_IssuerEnrollment) {
  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard(
      /*network=*/kVisaCard,
      /*virtual_card_enrollment_type=*/CreditCard::VirtualCardEnrollmentType::
          kIssuer));

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""),
                                         /*log_manager=*/nullptr);

  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the `VCN_MERCHANT_OPT_OUT_VISA` optimization type is not registered
// when we have seen a credit card form, but we do not have a virtual card on
// the account.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_VcnMerchantOptOut_NotEnrolledInVirtualCard) {
  payments_data_manager().AddServerCreditCard(test::GetMaskedServerCard());

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""),
                                         /*log_manager=*/nullptr);

  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that if the field type does not correlate to any optimization type we
// have, that no optimization type is registered.
TEST_F(AutofillOptimizationGuideTest, OptimizationTypeToRegisterNotFound) {
  AutofillField field;
  FormData form_data;
  form_data.set_fields({field});
  FormStructure form_structure{form_data};
  test_api(form_structure)
      .SetFieldTypes({MERCHANT_PROMO_CODE}, {MERCHANT_PROMO_CODE});

  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that if the form denotes that we need to register multiple optimization
// types, all of the optimization types that we need to register will be
// registered.
TEST_F(AutofillOptimizationGuideTest,
       FormWithMultipleOptimizationTypesToRegisterFound) {
  FormData form_data = CreateTestCreditCardFormData(/*is_https=*/true,
                                                    /*use_month_type=*/false);
  test_api(form_data).Append(CreateTestIbanFormData().fields());
  FormStructure form_structure{form_data};
  const std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NAME_LAST,        CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_MONTH,  CREDIT_CARD_EXP_4_DIGIT_YEAR, IBAN_VALUE};
  test_api(form_structure).SetFieldTypes(field_types, field_types);

  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard());

  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::IBAN_AUTOFILL_BLOCKED,
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA)));

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that single field suggestions are blocked when we are about to display
// suggestions for an IBAN field but the OptimizationGuideDecider denotes that
// displaying the suggestion is not allowed for the `IBAN_AUTOFILL_BLOCKED`
// optimization type.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockSingleFieldSuggestions_IbanAutofillBlocked) {
  FormStructure form_structure{CreateTestIbanFormData()};
  test_api(form_structure).SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});
  GURL url("https://example.com/");
  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(url), Eq(optimization_guide::proto::IBAN_AUTOFILL_BLOCKED),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(
      guide().ShouldBlockSingleFieldSuggestions(url, form_structure.field(0)));
}

// Test that single field suggestions are not blocked when we are about to
// display suggestions for an IBAN field and OptimizationGuideDecider denotes
// that displaying the suggestion is allowed for the `IBAN_AUTOFILL_BLOCKED`
// use-case.
TEST_F(AutofillOptimizationGuideTest,
       ShouldNotBlockSingleFieldSuggestions_IbanAutofillBlocked) {
  FormStructure form_structure{CreateTestIbanFormData()};
  test_api(form_structure).SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});
  GURL url("https://example.com/");
  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(url), Eq(optimization_guide::proto::IBAN_AUTOFILL_BLOCKED),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_FALSE(
      guide().ShouldBlockSingleFieldSuggestions(url, form_structure.field(0)));
}

// Test that single field suggestions are not blocked for the
// `IBAN_AUTOFILL_BLOCKED` use-case when the field is not an IBAN field.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockSingleFieldSuggestions_IbanAutofillBlocked_FieldTypeForBlockingNotFound) {
  FormStructure form_structure{CreateTestIbanFormData()};
  GURL url("https://example.com/");
  EXPECT_CALL(
      decider(),
      CanApplyOptimization(
          Eq(url), Eq(optimization_guide::proto::IBAN_AUTOFILL_BLOCKED),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .Times(0);

  EXPECT_FALSE(
      guide().ShouldBlockSingleFieldSuggestions(url, form_structure.field(0)));
}

// Test that blocking a virtual card suggestion works correctly in the VCN
// merchant opt-out use-case for Visa.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFormFieldSuggestion_VcnMerchantOptOutVisa) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCard();
  payments_data_manager().AddServerCreditCard(card);

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(url), Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(guide().ShouldBlockFormFieldSuggestion(url, card));
}

// Test that blocking a virtual card suggestion works correctly in the VCN
// merchant opt-out use-case for Discover.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFormFieldSuggestion_VcnMerchantOptOutDiscover) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCard(kDiscoverCard);
  payments_data_manager().AddServerCreditCard(card);

  ON_CALL(
      decider(),
      CanApplyOptimization(
          Eq(url), Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_DISCOVER),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(guide().ShouldBlockFormFieldSuggestion(url, card));
}

// Test that blocking a virtual card suggestion works correctly in the VCN
// merchant opt-out use-case for Mastercard.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFormFieldSuggestion_VcnMerchantOptOutMastercard) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCard(kMasterCard);
  payments_data_manager().AddServerCreditCard(card);

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(url),
              Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(guide().ShouldBlockFormFieldSuggestion(url, card));
}

// Test that if the URL is not blocklisted, we do not block a virtual card
// suggestion in the VCN merchant opt-out use-case.
TEST_F(AutofillOptimizationGuideTest,
       ShouldNotBlockFormFieldSuggestion_VcnMerchantOptOut_UrlNotBlocked) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCard();
  payments_data_manager().AddServerCreditCard(card);

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(url), Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_FALSE(guide().ShouldBlockFormFieldSuggestion(url, card));
}

// Test that we do not block virtual card suggestions in the VCN merchant
// opt-out use-case if the card is an issuer-level enrollment.
TEST_F(AutofillOptimizationGuideTest,
       ShouldNotBlockFormFieldSuggestion_VcnMerchantOptOut_IssuerEnrollment) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCard(
      /*network=*/kVisaCard, /*virtual_card_enrollment_type=*/CreditCard::
          VirtualCardEnrollmentType::kIssuer);
  payments_data_manager().AddServerCreditCard(card);

  EXPECT_CALL(
      decider(),
      CanApplyOptimization(
          Eq(url), Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .Times(0);

  EXPECT_FALSE(guide().ShouldBlockFormFieldSuggestion(url, card));
}

// Test that we do not block the virtual card suggestion from being shown in the
// VCN merchant opt-out use-case if the network does not have a VCN merchant
// opt-out blocklist.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockFormFieldSuggestion_VcnMerchantOptOut_NetworkDoesNotHaveBlocklist) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCard(/*network=*/kAmericanExpressCard);
  payments_data_manager().AddServerCreditCard(card);

  EXPECT_CALL(
      decider(),
      CanApplyOptimization(
          Eq(url), Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .Times(0);

  EXPECT_FALSE(guide().ShouldBlockFormFieldSuggestion(url, card));
}

// Test that we block card flat rate benefits suggestions on blocked URLs.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFlatRateBenefitSuggestionLabelsForUrl_BlockedUrl) {
  GURL url("https://example.com/");

  MockFlatRateCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kFalse);

  EXPECT_TRUE(guide().ShouldBlockFlatRateBenefitSuggestionLabelsForUrl(url));
}

// Test that we do not block card flat rate benefits suggestions on unblocked
// URLs.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFlatRateBenefitSuggestionLabelsForUrl_UnblockedUrl) {
  GURL url("https://example.com/");

  MockFlatRateCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kTrue);

  EXPECT_FALSE(guide().ShouldBlockFlatRateBenefitSuggestionLabelsForUrl(url));
}

// Test that we do not block benefits suggestions when a `kUnknown` decision is
// returned.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFlatRateBenefitSuggestionLabelsForUrl_UnknownDecision) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCard(
      kVisaCard, CreditCard::VirtualCardEnrollmentType::kNetwork,
      kCapitalOneCardIssuerId);
  payments_data_manager().AddServerCreditCard(card);

  MockFlatRateCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kUnknown);

  EXPECT_FALSE(guide().ShouldBlockFlatRateBenefitSuggestionLabelsForUrl(url));
}

// Test that the Amex category-benefit optimization types are registered when we
// have seen a credit card form and the user has an Amex card.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmexCategoryBenefits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCardBenefitsSync,
                            features::kAutofillEnableCardBenefitsSourceSync},
      /*disabled_features=*/{});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard(
      /*network=*/kAmericanExpressCard,
      /*virtual_card_enrollment_type=*/
      CreditCard::VirtualCardEnrollmentType::kNetwork,
      /*issuer_id=*/kAmexCardIssuerId,
      /*benefit_source=*/kAmexCardBenefitSource));

  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(UnorderedElementsAre(
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS,
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS)));

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the flat rate benefit blocklist optimization type is registered
// when we have seen a credit card form and the user has a card with a flat rate
// benefit.
TEST_F(
    AutofillOptimizationGuideTest,
    CreditCardFormFound_FlatRateBenefitBlockList_WithFlatRateBenefit_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCardBenefitsSync,
                            features::
                                kAutofillEnableFlatRateCardBenefitsBlocklist},
      /*disabled_features=*/{});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  CreditCard card = test::GetMaskedServerCard();
  payments_data_manager().AddServerCreditCard(card);
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit)
      .SetLinkedCardInstrumentId(
          CreditCardBenefitBase::LinkedCardInstrumentId(card.instrument_id()));
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(flat_rate_benefit));

  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::
                      SHARED_CREDIT_CARD_FLAT_RATE_BENEFITS_BLOCKLIST)));

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the flat rate benefit blocklist optimization type is not
// registered when we have seen a credit card form but the user has no card
// with a flat rate benefit.
TEST_F(
    AutofillOptimizationGuideTest,
    CreditCardFormFound_FlatRateBenefitBlockList_WithoutFlatRateBenefit_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCardBenefitsSync,
                            features::
                                kAutofillEnableFlatRateCardBenefitsBlocklist},
      /*disabled_features=*/{});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  payments_data_manager().AddServerCreditCard(test::GetMaskedServerCard());

  // The flat rate blocklist optimization type will not be registered if the
  // no card has a flat rate benefit.
  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the flat rate benefit blocklist optimization type is not registered
// when we have seen a credit card form and the user has a card with flat rate
// benefit, but the flat rate benefit blocklist flag is disabled.
TEST_F(
    AutofillOptimizationGuideTest,
    CreditCardFormFound_FlatRateBenefitBlockList_WithFlatRateBenefit_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCardBenefitsSync},
      /*disabled_features=*/{
          features::kAutofillEnableFlatRateCardBenefitsBlocklist});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  CreditCard card = test::GetMaskedServerCard();
  payments_data_manager().AddServerCreditCard(card);
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit)
      .SetLinkedCardInstrumentId(
          CreditCardBenefitBase::LinkedCardInstrumentId(card.instrument_id()));
  payments_data_manager().AddCreditCardBenefitForTest(
      std::move(flat_rate_benefit));

  // The flat rate blocklist optimization type will not be registered if the
  // blocklist flag is disabled.
  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the BMO category-benefit optimization types are registered when a
// credit card form is present and the user has an BMO card.
TEST_F(AutofillOptimizationGuideTest, CreditCardFormFound_BmoCategoryBenefits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/
      {features::kAutofillEnableCardBenefitsSync,
       features::kAutofillEnableAllowlistForBmoCardCategoryBenefits,
       features::kAutofillEnableCardBenefitsSourceSync},
      /*disabled_features=*/{});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard(
      /*network=*/kMasterCard,
      /*virtual_card_enrollment_type=*/
      CreditCard::VirtualCardEnrollmentType::kNetwork,
      /*issuer_id=*/kBmoCardIssuerId,
      /*benefit_source=*/kBmoCardBenefitSource));

  EXPECT_CALL(
      decider(),
      RegisterOptimizationTypes(UnorderedElementsAre(
          optimization_guide::proto::BMO_CREDIT_CARD_AIR_MILES_PARTNER_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_ALCOHOL_STORE_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_DINING_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_DRUGSTORE_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_ENTERTAINMENT_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_GROCERY_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_OFFICE_SUPPLY_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_RECURRING_BILL_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_TRANSIT_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_TRAVEL_BENEFITS,
          optimization_guide::proto::BMO_CREDIT_CARD_WHOLESALE_CLUB_BENEFITS,
          optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD)));

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the Amex category-benefit optimization types are not registered
// when the `kAutofillEnableCardBenefitsSync` experiment is disabled.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmexCategoryBenefits_ExperimentDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCardBenefitsSourceSync},
      /*disabled_features=*/{features::kAutofillEnableCardBenefitsSync});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard(
      /*network=*/kAmericanExpressCard,
      /*virtual_card_enrollment_type=*/
      CreditCard::VirtualCardEnrollmentType::kNetwork,
      /*issuer_id=*/kAmexCardIssuerId,
      /*benefit_source=*/kAmexCardBenefitSource));

  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(UnorderedElementsAre(
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS,
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS)))
      .Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that the BMO category-benefit optimization types are not registered when
// the `kAutofillEnableAllowlistForBmoCardCategoryBenefits` experiment is
// disabled.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_BmoCategoryBenefits_ExperimentDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableCardBenefitsSourceSync},
      /*disabled_features=*/{
          features::kAutofillEnableAllowlistForBmoCardCategoryBenefits});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  payments_data_manager().AddServerCreditCard(GetVcnEnrolledCard(
      /*network=*/kMasterCard,
      /*virtual_card_enrollment_type=*/
      CreditCard::VirtualCardEnrollmentType::kNetwork,
      /*issuer_id=*/kBmoCardIssuerId,
      /*benefit_source=*/kBmoCardBenefitSource));

  // Since the experiment is disabled, there should be no benefits-related
  // optimization types registered.
  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(UnorderedElementsAre(
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD)));

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test the `BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM` optimization type is registered
// when the amount extraction allowlist is enabled and there is at least one
// Affirm BNPL issuer.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(
    AutofillOptimizationGuideTest,
    CreditCardFormFound_AmountExtractionAllowed_BuyNowPayLaterProviderAffirm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillEnableAmountExtractionAllowlistDesktop,
       features::kAutofillEnableBuyNowPayLaterSyncing},
      {});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  BnplIssuer bnpl_issuer = test::GetTestLinkedBnplIssuer();
  bnpl_issuer.set_issuer_id(BnplIssuer::IssuerId::kBnplAffirm);
  payments_data_manager().AddBnplIssuer(bnpl_issuer);

  // Ensure that on registration the right optimization type is registered.
  EXPECT_CALL(
      decider(),
      RegisterOptimizationTypes(testing::IsSupersetOf(
          {optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM})));
  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test the `BUY_NOW_PAY_LATER_ALLOWLIST_ZIP` optimization type is registered
// when the amount extraction allowlist is enabled and there is at least one
// Zip BNPL issuer.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmountExtractionAllowed_BuyNowPayLaterProviderZip) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillEnableAmountExtractionAllowlistDesktop,
       features::kAutofillEnableBuyNowPayLaterSyncing},
      {});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  BnplIssuer bnpl_issuer = test::GetTestLinkedBnplIssuer();
  bnpl_issuer.set_issuer_id(BnplIssuer::IssuerId::kBnplZip);
  payments_data_manager().AddBnplIssuer(bnpl_issuer);

  // Ensure that on registration the right optimization type is registered.
  EXPECT_CALL(
      decider(),
      RegisterOptimizationTypes(testing::IsSupersetOf(
          {optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP})));
  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test neither `BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM` nor
// `BUY_NOW_PAY_LATER_ALLOWLIST_ZIP` optimization types are registered when the
// amount extraction allowlist flag is off.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmountExtractionAllowed_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{
          features::kAutofillEnableAmountExtractionAllowlistDesktop});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  BnplIssuer bnpl_issuer = test::GetTestLinkedBnplIssuer();
  bnpl_issuer.set_issuer_id(BnplIssuer::IssuerId::kBnplAffirm);
  payments_data_manager().AddBnplIssuer(bnpl_issuer);

  // RegisterOptimizationTypes shouldn't be called.
  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test neither `BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM` nor
// `BUY_NOW_PAY_LATER_ALLOWLIST_ZIP` optimization types are registered when
// there is no BNPL issuer synced to the account.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmountExtractionAllowed_NoBnplIssuerFound) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillEnableAmountExtractionAllowlistDesktop,
       features::kAutofillEnableBuyNowPayLaterSyncing},
      {});
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});

  // RegisterOptimizationTypes shouldn't be called.
  EXPECT_CALL(decider(), RegisterOptimizationTypes).Times(0);

  guide().OnDidParseForm(form_structure, payments_data_manager());
}

// Test that we allow checkout amount searching for Affirm on an allowlisted
// URL.
TEST_F(AutofillOptimizationGuideTest,
       IsUrlEligibleForBnplIssuer_AffirmUrlAllowed) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionAllowlistDesktop};

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(GURL("https://www.testurl.test")),
              Eq(optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));

  // testurl.test is in the allowlist.
  EXPECT_TRUE(guide().IsUrlEligibleForBnplIssuer(
      BnplIssuer::IssuerId::kBnplAffirm, GURL("https://www.testurl.test")));
}

// Test that we do not allow checkout amount searching for Affirm on a
// non-allowlisted URL.
TEST_F(AutofillOptimizationGuideTest,
       IsUrlEligibleForBnplIssuer_AffirmUrlBlocked) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionAllowlistDesktop};

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(GURL("https://www.testurl.test")),
              Eq(optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  // testurl.test is not in the allowlist.
  EXPECT_FALSE(guide().IsUrlEligibleForBnplIssuer(
      BnplIssuer::IssuerId::kBnplAffirm, GURL("https://www.testurl.test")));
}

// Test that we allow checkout amount searching for Zip on an allowlisted URL.
TEST_F(AutofillOptimizationGuideTest,
       IsUrlEligibleForBnplIssuer_ZipUrlAllowed) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionAllowlistDesktop};

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(GURL("https://www.testurl.test")),
              Eq(optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));

  // testurl.test is in the allowlist.
  EXPECT_TRUE(guide().IsUrlEligibleForBnplIssuer(
      BnplIssuer::IssuerId::kBnplZip, GURL("https://www.testurl.test")));
}

// Test that we do not allow checkout amount searching for Zip on a
// non-allowlisted URL.
TEST_F(AutofillOptimizationGuideTest,
       IsUrlEligibleForBnplIssuer_ZipUrlBlocked) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionAllowlistDesktop};

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(GURL("https://www.testurl.test")),
              Eq(optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));

  // testurl.test is not in the allowlist.
  EXPECT_FALSE(guide().IsUrlEligibleForBnplIssuer(
      BnplIssuer::IssuerId::kBnplZip, GURL("https://www.testurl.test")));
}

// Test that we allow checkout with BNPL for Affirm on a non-allowlisted URL
// when AmountExtractionDesktopLogging is enabled.
TEST_F(
    AutofillOptimizationGuideTest,
    IsUrlEligibleForBnplIssuer_AmountExtractionDesktopLoggingEnabled_AffirmUrlAllowed) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionTesting};

  EXPECT_CALL(
      decider(),
      CanApplyOptimization(
          Eq(GURL("https://www.testurl.test")),
          Eq(optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .Times(0);

  // testurl.test is not in the allowlist, but
  // kAutofillEnableAmountExtractionTesting overrides the allowlist.
  EXPECT_TRUE(guide().IsUrlEligibleForBnplIssuer(
      BnplIssuer::IssuerId::kBnplAffirm, GURL("https://www.testurl.test")));
}

// Test that we allow checkout with BNPL for Zip on a non-allowlisted URL when
// AmountExtractionDesktopLogging is enabled.
TEST_F(
    AutofillOptimizationGuideTest,
    IsUrlEligibleForBnplIssuer_AmountExtractionDesktopLoggingEnabled_ZipUrlAllowed) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionTesting};

  EXPECT_CALL(
      decider(),
      CanApplyOptimization(
          Eq(GURL("https://www.testurl.test")),
          Eq(optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP),
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .Times(0);

  // testurl.test is not in the allowlist, but
  // kAutofillEnableAmountExtractionTesting overrides the allowlist.
  EXPECT_TRUE(guide().IsUrlEligibleForBnplIssuer(
      BnplIssuer::IssuerId::kBnplZip, GURL("https://www.testurl.test")));
}

// Test that we do not allow checkout amount searching when the amount
// extraction allowlist is off.
TEST_F(AutofillOptimizationGuideTest,
       IsUrlEligibleForBnplIssuer_AllowlistFlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnableAmountExtractionAllowlistDesktop);

  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(GURL("https://www.testurl.test")),
              Eq(optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_FALSE(guide().IsUrlEligibleForBnplIssuer(
      BnplIssuer::IssuerId::kBnplZip, GURL("https://www.testurl.test")));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Test that the ablation site lists are registered in case the ablation
// experiment is enabled.
TEST_F(AutofillOptimizationGuideTest, AutofillAblation) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAblationStudy};
  FormData form_data = CreateTestCreditCardFormData(/*is_https=*/true,
                                                    /*use_month_type=*/false);
  FormStructure form_structure{form_data};
  const std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NAME_LAST, CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_4_DIGIT_YEAR};
  test_api(form_structure).SetFieldTypes(field_types, field_types);

  // Ensure that on registration the right optimization types are registered.
  EXPECT_CALL(decider(),
              RegisterOptimizationTypes(testing::IsSupersetOf(
                  {optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST2,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST3,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST4,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST5,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST6})));
  guide().OnDidParseForm(form_structure, payments_data_manager());

  // Ensure that `IsEligibleForAblation()` returns the right responses.
  ON_CALL(decider(),
          CanApplyOptimization(
              _, _,
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kFalse));
  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(GURL("https://www.example.com")),
              Eq(optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));
  EXPECT_CALL(
      decider(),
      CanApplyOptimization(
          _, _,
          Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .Times(3);
  EXPECT_TRUE(guide().IsEligibleForAblation(
      GURL("https://www.example.com"),
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1));
  // www.othersite.com is not on any list.
  EXPECT_FALSE(guide().IsEligibleForAblation(
      GURL("https://www.othersite.com"),
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1));
  // www.example.com is not on list 2, but on list 1.
  EXPECT_FALSE(guide().IsEligibleForAblation(
      GURL("https://www.example.com"),
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST2));
}

struct BenefitOptimizationToBenefitCategoryTestCase {
  const std::string benefit_source;
  const optimization_guide::proto::OptimizationType optimization_type;
  const CreditCardCategoryBenefit::BenefitCategory benefit_category;
};

class BenefitOptimizationToBenefitCategoryTest
    : public AutofillOptimizationGuideTest,
      public testing::WithParamInterface<
          BenefitOptimizationToBenefitCategoryTestCase> {
 public:
  BenefitOptimizationToBenefitCategoryTest() = default;

  ~BenefitOptimizationToBenefitCategoryTest() override = default;

  optimization_guide::proto::OptimizationType expected_benefit_optimization()
      const {
    return GetParam().optimization_type;
  }
  CreditCardCategoryBenefit::BenefitCategory expected_benefit_category() const {
    return GetParam().benefit_category;
  }

  const CreditCard& credit_card() const { return card_; }

  void SetUp() override {
    AutofillOptimizationGuideTest::SetUp();
    card_ = test::GetMaskedServerCard();
    card_.set_benefit_source(GetParam().benefit_source);
    payments_data_manager().AddServerCreditCard(card_);
  }

 private:
  CreditCard card_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnableCardBenefitsSourceSync};
};

// Tests that the correct benefit category is returned when a benefit
// optimization is found for a particular credit card issuer and url.
TEST_P(BenefitOptimizationToBenefitCategoryTest,
       GetBenefitCategoryForOptimizationType) {
  GURL url = GURL("https://example.com/");
  ON_CALL(decider(),
          CanApplyOptimization(
              Eq(url), Eq(expected_benefit_optimization()),
              Matcher<optimization_guide::OptimizationMetadata*>(Eq(nullptr))))
      .WillByDefault(
          Return(optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_EQ(guide().AttemptToGetEligibleCreditCardBenefitCategory(
                credit_card().benefit_source(), url),
            expected_benefit_category());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BenefitOptimizationToBenefitCategoryTest,
    testing::Values(
        BenefitOptimizationToBenefitCategoryTestCase{
            "amex",
            optimization_guide::proto::
                AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kFlights},
        BenefitOptimizationToBenefitCategoryTestCase{
            "amex",
            optimization_guide::proto::
                AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kSubscription},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo",
            optimization_guide::proto::
                BMO_CREDIT_CARD_AIR_MILES_PARTNER_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kAirMilesPartner},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo",
            optimization_guide::proto::BMO_CREDIT_CARD_ALCOHOL_STORE_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kAlcoholStores},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo", optimization_guide::proto::BMO_CREDIT_CARD_DINING_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kDining},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo",
            optimization_guide::proto::BMO_CREDIT_CARD_DRUGSTORE_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kDrugstores},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo",
            optimization_guide::proto::BMO_CREDIT_CARD_ENTERTAINMENT_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kEntertainment},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo", optimization_guide::proto::BMO_CREDIT_CARD_GROCERY_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kGroceryStores},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo",
            optimization_guide::proto::BMO_CREDIT_CARD_OFFICE_SUPPLY_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kOfficeSupplies},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo",
            optimization_guide::proto::BMO_CREDIT_CARD_RECURRING_BILL_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kRecurringBills},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo", optimization_guide::proto::BMO_CREDIT_CARD_TRANSIT_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kTransit},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo", optimization_guide::proto::BMO_CREDIT_CARD_TRAVEL_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kTravel},
        BenefitOptimizationToBenefitCategoryTestCase{
            "bmo",
            optimization_guide::proto::BMO_CREDIT_CARD_WHOLESALE_CLUB_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kWholesaleClubs}));

}  // namespace autofill
