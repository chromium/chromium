// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_optimization_guide.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_test_api.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

using test::CreateTestCreditCardFormData;
using test::CreateTestIbanFormData;

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<optimization_guide::proto::OptimizationType>&,
       optimization_guide::proto::RequestContext,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback,
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata),
      (override));
};

class AutofillOptimizationGuideTest : public testing::Test {
 public:
  AutofillOptimizationGuideTest()
      : pref_service_(test::PrefServiceForTesting()),
        decider_(std::make_unique<MockOptimizationGuideDecider>()),
        personal_data_manager_(std::make_unique<TestPersonalDataManager>()),
        autofill_optimization_guide_(
            std::make_unique<AutofillOptimizationGuide>(decider_.get())) {
    personal_data_manager_->SetPrefService(pref_service_.get());
    personal_data_manager_->SetSyncServiceForTest(&sync_service_);
  }

  CreditCard GetVcnEnrolledCardForMerchantOptOut(
      std::string_view network = kVisaCard,
      CreditCard::VirtualCardEnrollmentType virtual_card_enrollment_type =
          CreditCard::VirtualCardEnrollmentType::kNetwork,
      std::string_view issuer_id = "") {
    CreditCard card = test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
    test_api(card).set_network_for_card(network);
    card.set_virtual_card_enrollment_type(virtual_card_enrollment_type);
    test_api(card).set_issuer_id_for_card(issuer_id);
    return card;
  }

  void MockCapitalOneCreditCardBenefitsBlockedDecisionForUrl(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision) {
    ON_CALL(*decider_,
            CanApplyOptimization(
                testing::Eq(url),
                testing::Eq(optimization_guide::proto::
                                CAPITAL_ONE_CREDIT_CARD_BENEFITS_BLOCKED),
                testing::Matcher<optimization_guide::OptimizationMetadata*>(
                    testing::Eq(nullptr))))
        .WillByDefault(testing::Return(decision));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<PrefService> pref_service_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<MockOptimizationGuideDecider> decider_;
  std::unique_ptr<TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<AutofillOptimizationGuide> autofill_optimization_guide_;
};

TEST_F(AutofillOptimizationGuideTest, EnsureIntegratorInitializedCorrectly) {
  EXPECT_TRUE(autofill_optimization_guide_
                  ->GetOptimizationGuideKeyedServiceForTesting() ==
              decider_.get());
}

// Test that the `IBAN_AUTOFILL_BLOCKED` optimization type is registered when we
// have seen an IBAN form.
TEST_F(AutofillOptimizationGuideTest, IbanFieldFound_IbanAutofillBlocked) {
  FormStructure form_structure{CreateTestIbanFormData()};
  test_api(form_structure).SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  EXPECT_CALL(*decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::IBAN_AUTOFILL_BLOCKED)));

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that the corresponding optimization types are registered in the VCN
// merchant opt-out case when a credit card form is seen, and VCNs that have an
// associated optimization guide blocklist are present.
TEST_F(AutofillOptimizationGuideTest, CreditCardFormFound_VcnMerchantOptOut) {
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut());
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(kDiscoverCard));
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(kMasterCard));

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(
      GeoIpCountryCode(""),
      /*form_interactions_ukm_logger=*/nullptr, /*log_manager=*/nullptr);

  EXPECT_CALL(*decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA,
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_DISCOVER,
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD)));

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that the `VCN_MERCHANT_OPT_OUT_VISA` optimization type is not registered
// when we have seen a credit card form, but the network is not Visa.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_VcnMerchantOptOut_NotVisaNetwork) {
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(/*network=*/kAmericanExpressCard));

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(
      GeoIpCountryCode(""),
      /*form_interactions_ukm_logger=*/nullptr, /*log_manager=*/nullptr);

  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that the `VCN_MERCHANT_OPT_OUT_VISA` optimization type is not registered
// when we have seen a credit card form, but the virtual card is an issuer-level
// enrollment
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_VcnMerchantOptOut_IssuerEnrollment) {
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(
          /*network=*/kVisaCard,
          /*virtual_card_enrollment_type=*/CreditCard::
              VirtualCardEnrollmentType::kIssuer));

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(
      GeoIpCountryCode(""),
      /*form_interactions_ukm_logger=*/nullptr, /*log_manager=*/nullptr);

  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that the `VCN_MERCHANT_OPT_OUT_VISA` optimization type is not registered
// when we have seen a credit card form, but we do not have a virtual card on
// the account.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_VcnMerchantOptOut_NotEnrolledInVirtualCard) {
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      test::GetMaskedServerCard());

  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(
      GeoIpCountryCode(""),
      /*form_interactions_ukm_logger=*/nullptr, /*log_manager=*/nullptr);

  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that no optimization type is registered when we have seen a credit card
// form, and meet all of the pre-requisites for the Visa merchant opt-out
// use-case, but there is no personal data manager present.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_VcnMerchantOptOut_NoPersonalDataManager) {
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  form_structure.DetermineHeuristicTypes(
      GeoIpCountryCode(""),
      /*form_interactions_ukm_logger=*/nullptr, /*log_manager=*/nullptr);
  personal_data_manager_.reset();

  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
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

  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
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

  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut());

  EXPECT_CALL(*decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::IBAN_AUTOFILL_BLOCKED,
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA)));

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
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
  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(optimization_guide::proto::IBAN_AUTOFILL_BLOCKED),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(autofill_optimization_guide_->ShouldBlockSingleFieldSuggestions(
      url, form_structure.field(0)));
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
  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(optimization_guide::proto::IBAN_AUTOFILL_BLOCKED),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_FALSE(autofill_optimization_guide_->ShouldBlockSingleFieldSuggestions(
      url, form_structure.field(0)));
}

// Test that single field suggestions are not blocked for the
// `IBAN_AUTOFILL_BLOCKED` use-case when the field is not an IBAN field.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockSingleFieldSuggestions_IbanAutofillBlocked_FieldTypeForBlockingNotFound) {
  FormStructure form_structure{CreateTestIbanFormData()};
  GURL url("https://example.com/");
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  testing::Eq(url),
                  testing::Eq(optimization_guide::proto::IBAN_AUTOFILL_BLOCKED),
                  testing::Matcher<optimization_guide::OptimizationMetadata*>(
                      testing::Eq(nullptr))))
      .Times(0);

  EXPECT_FALSE(autofill_optimization_guide_->ShouldBlockSingleFieldSuggestions(
      url, form_structure.field(0)));
}

// Test that blocking a virtual card suggestion works correctly in the VCN
// merchant opt-out use-case for Visa.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFormFieldSuggestion_VcnMerchantOptOutVisa) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut();
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(
      autofill_optimization_guide_->ShouldBlockFormFieldSuggestion(url, card));
}

// Test that blocking a virtual card suggestion works correctly in the VCN
// merchant opt-out use-case for Discover.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFormFieldSuggestion_VcnMerchantOptOutDiscover) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(kDiscoverCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  ON_CALL(
      *decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_DISCOVER),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(
      autofill_optimization_guide_->ShouldBlockFormFieldSuggestion(url, card));
}

// Test that blocking a virtual card suggestion works correctly in the VCN
// merchant opt-out use-case for Mastercard.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockFormFieldSuggestion_VcnMerchantOptOutMastercard) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(kMasterCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_TRUE(
      autofill_optimization_guide_->ShouldBlockFormFieldSuggestion(url, card));
}

// Test that if the URL is not blocklisted, we do not block a virtual card
// suggestion in the VCN merchant opt-out use-case.
TEST_F(AutofillOptimizationGuideTest,
       ShouldNotBlockFormFieldSuggestion_VcnMerchantOptOut_UrlNotBlocked) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut();
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(url),
              testing::Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_FALSE(
      autofill_optimization_guide_->ShouldBlockFormFieldSuggestion(url, card));
}

// Test that we do not block virtual card suggestions in the VCN merchant
// opt-out use-case if the card is an issuer-level enrollment.
TEST_F(AutofillOptimizationGuideTest,
       ShouldNotBlockFormFieldSuggestion_VcnMerchantOptOut_IssuerEnrollment) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(
      /*network=*/kVisaCard, /*virtual_card_enrollment_type=*/CreditCard::
          VirtualCardEnrollmentType::kIssuer);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  EXPECT_CALL(
      *decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(0);

  EXPECT_FALSE(
      autofill_optimization_guide_->ShouldBlockFormFieldSuggestion(url, card));
}

// Test that we do not block the virtual card suggestion from being shown in the
// VCN merchant opt-out use-case if the network does not have a VCN merchant
// opt-out blocklist.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockFormFieldSuggestion_VcnMerchantOptOut_NetworkDoesNotHaveBlocklist) {
  GURL url("https://example.com/");
  CreditCard card =
      GetVcnEnrolledCardForMerchantOptOut(/*network=*/kAmericanExpressCard);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  EXPECT_CALL(
      *decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(optimization_guide::proto::VCN_MERCHANT_OPT_OUT_VISA),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(0);

  EXPECT_FALSE(
      autofill_optimization_guide_->ShouldBlockFormFieldSuggestion(url, card));
}

// Test that we block benefits suggestions for Capital One cards on blocked
// URLs.
TEST_F(AutofillOptimizationGuideTest,
       ShouldBlockBenefitSuggestionLabelsForCardAndUrl_CapitalOne_BlockedUrl) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(
      kVisaCard, CreditCard::VirtualCardEnrollmentType::kNetwork,
      kCapitalOneCardIssuerId);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  MockCapitalOneCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kFalse);

  EXPECT_TRUE(autofill_optimization_guide_
                  ->ShouldBlockBenefitSuggestionLabelsForCardAndUrl(card, url));
}

// Test that we do not block benefits suggestions for Capital One cards on
// unblocked URLs.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockBenefitSuggestionLabelsForCardAndUrl_CapitalOne_UnblockedUrl) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(
      kVisaCard, CreditCard::VirtualCardEnrollmentType::kNetwork,
      kCapitalOneCardIssuerId);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  MockCapitalOneCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kTrue);

  EXPECT_FALSE(
      autofill_optimization_guide_
          ->ShouldBlockBenefitSuggestionLabelsForCardAndUrl(card, url));
}

// Test that we do not block benefits suggestions when a kUnknown decision is
// returned.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockBenefitSuggestionLabelsForCardAndUrl_CapitalOne_UnknownDecision) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(
      kVisaCard, CreditCard::VirtualCardEnrollmentType::kNetwork,
      kCapitalOneCardIssuerId);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  MockCapitalOneCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kUnknown);

  EXPECT_FALSE(
      autofill_optimization_guide_
          ->ShouldBlockBenefitSuggestionLabelsForCardAndUrl(card, url));
}

// Test that we do not block benefits suggestions for non-Capital One cards on
// blocked URLs.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockBenefitSuggestionLabelsForCardAndUrl_NonCapitalOne_BlockedUrl) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(
      /*network=*/kAmericanExpressCard, /*virtual_card_enrollment_type=*/
      CreditCard::VirtualCardEnrollmentType::kNetwork,
      /*issuer_id=*/kAmexCardIssuerId);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  MockCapitalOneCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kFalse);

  EXPECT_FALSE(
      autofill_optimization_guide_
          ->ShouldBlockBenefitSuggestionLabelsForCardAndUrl(card, url));
}

// Test that we do not block benefits suggestions for non-Capital One cards on
// unblocked URLs.
TEST_F(
    AutofillOptimizationGuideTest,
    ShouldNotBlockBenefitSuggestionLabelsForCardAndUrl_NonCapitalOne_UnblockedUrl) {
  GURL url("https://example.com/");
  CreditCard card = GetVcnEnrolledCardForMerchantOptOut(
      /*network=*/kAmericanExpressCard, /*virtual_card_enrollment_type=*/
      CreditCard::VirtualCardEnrollmentType::kNetwork,
      /*issuer_id=*/kAmexCardIssuerId);
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      card);

  MockCapitalOneCreditCardBenefitsBlockedDecisionForUrl(
      url, optimization_guide::OptimizationGuideDecision::kTrue);

  EXPECT_FALSE(
      autofill_optimization_guide_
          ->ShouldBlockBenefitSuggestionLabelsForCardAndUrl(card, url));
}

// Test that the Amex category-benefit optimization types are registered when we
// have seen a credit card form and the user has an Amex card.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmexCategoryBenefits) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableCardBenefitsSync};
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(
          /*network=*/kAmericanExpressCard,
          /*virtual_card_enrollment_type=*/
          CreditCard::VirtualCardEnrollmentType::kNetwork,
          /*issuer_id=*/kAmexCardIssuerId));

  EXPECT_CALL(*decider_,
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS,
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS)));

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that the Capital One category-benefit optimization types are registered
// when we have seen a credit card form and the user has a Capital One card.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_CapitalOneCategoryBenefits) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableCardBenefitsSync};
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(
          /*network=*/kMasterCard,
          /*virtual_card_enrollment_type=*/
          CreditCard::VirtualCardEnrollmentType::kNetwork,
          /*issuer_id=*/kCapitalOneCardIssuerId));

  EXPECT_CALL(
      *decider_,
      RegisterOptimizationTypes(testing::UnorderedElementsAre(
          optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_DINING_BENEFITS,
          optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_GROCERY_BENEFITS,
          optimization_guide::proto::
              CAPITAL_ONE_CREDIT_CARD_ENTERTAINMENT_BENEFITS,
          optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_STREAMING_BENEFITS,
          optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_BENEFITS_BLOCKED,
          optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD)));

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that the Amex category-benefit optimization types are not registered
// when the kAutofillEnableCardBenefitsSync experiment is disabled.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmexCategoryBenefits_ExperimentDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillEnableCardBenefitsSync);
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(
          /*network=*/kAmericanExpressCard,
          /*virtual_card_enrollment_type=*/
          CreditCard::VirtualCardEnrollmentType::kNetwork,
          /*issuer_id=*/kAmexCardIssuerId));

  EXPECT_CALL(*decider_,
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS,
                  optimization_guide::proto::
                      AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS)))
      .Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that the Capital One category-benefit optimization types are not
// registered when the kAutofillEnableCardBenefitsSync experiment is disabled.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_CapitalOneCategoryBenefits_ExperimentDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillEnableCardBenefitsSync);
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      GetVcnEnrolledCardForMerchantOptOut(
          /*network=*/kMasterCard,
          /*virtual_card_enrollment_type=*/
          CreditCard::VirtualCardEnrollmentType::kNetwork,
          /*issuer_id=*/kCapitalOneCardIssuerId));

  // Since the experiment is disabled, there should be no benefits-related
  // optimization types registered.
  EXPECT_CALL(*decider_,
              RegisterOptimizationTypes(testing::UnorderedElementsAre(
                  optimization_guide::proto::VCN_MERCHANT_OPT_OUT_MASTERCARD)))
      .Times(1);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test the `BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM` optimization type is registered
// when the amount extraction experiment is enabled and there is at least one
// server credit card.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(
    AutofillOptimizationGuideTest,
    CreditCardFormFound_AmountExtractionAllowed_BuyNowPayLaterProviderAffirm) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionDesktop};
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      test::GetMaskedServerCard());

  // Ensure that on registration the right optimization type is registered.
  EXPECT_CALL(
      *decider_,
      RegisterOptimizationTypes(testing::IsSupersetOf(
          {optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM})));
  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test the `BUY_NOW_PAY_LATER_ALLOWLIST_ZIP` optimization type is registered
// when the amount extraction experiment is enabled and there is at least one
// server credit card.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmountExtractionAllowed_BuyNowPayLaterProviderZip) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionDesktop};
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      test::GetMaskedServerCard());

  // Ensure that on registration the right optimization type is registered.
  EXPECT_CALL(
      *decider_,
      RegisterOptimizationTypes(testing::IsSupersetOf(
          {optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP})));
  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test neither `BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM` nor
// `BUY_NOW_PAY_LATER_ALLOWLIST_ZIP` optimization types are registered when the
// amount extraction experiment is off.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmountExtractionAllowed_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnableAmountExtractionDesktop);
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
      test::GetMaskedServerCard());

  // RegisterOptimizationTypes shouldn't be called.
  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test neither `BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM` nor
// `BUY_NOW_PAY_LATER_ALLOWLIST_ZIP` optimization types are registered when
// there is no server credit card.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmountExtractionAllowed_NoServerCreditCardFound) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionDesktop};
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_->test_payments_data_manager().AddCreditCard(
      test::GetCreditCard());

  // RegisterOptimizationTypes shouldn't be called.
  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test neither `BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM` nor
// `BUY_NOW_PAY_LATER_ALLOWLIST_ZIP` optimization types are registered when
// there is no personal data manager present.
TEST_F(AutofillOptimizationGuideTest,
       CreditCardFormFound_AmountExtractionAllowed_NoPersonalDataManager) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableAmountExtractionDesktop};
  FormStructure form_structure{
      CreateTestCreditCardFormData(/*is_https=*/true,
                                   /*use_month_type=*/true)};
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_VERIFICATION_CODE});
  personal_data_manager_.reset();

  // RegisterOptimizationTypes shouldn't be called.
  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());
}

// Test that we allow BNPL for Affirm on an allowlisted URL.
TEST_F(AutofillOptimizationGuideTest,
       IsEligibleForBuyNowPayLater_AffirmUrlAllowed) {
  // Ensure that `IsEligibleForBuyNowPayLater()` returns the right
  // response.
  ON_CALL(
      *decider_,
      CanApplyOptimization(
          testing::Eq(GURL("https://www.abercrombie.com")),
          testing::Eq(
              optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  // abercrombie.com is in the allowlist.
  EXPECT_TRUE(autofill_optimization_guide_->IsEligibleForBuyNowPayLater(
      /*issuer_id=*/"affirm", GURL("https://www.abercrombie.com")));
}

// Test that we do not allow BNPL for Affirm on a non-allowlisted URL.
TEST_F(AutofillOptimizationGuideTest,
       IsEligibleForBuyNowPayLater_AffirmUrlBlocked) {
  // Ensure that `IsEligibleForBuyNowPayLater()` returns the right
  // response.
  ON_CALL(
      *decider_,
      CanApplyOptimization(
          testing::Eq(GURL("https://www.abc.com")),
          testing::Eq(
              optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  // abc.com is not in the allowlist.
  EXPECT_FALSE(autofill_optimization_guide_->IsEligibleForBuyNowPayLater(
      /*issuer_id=*/"affirm", GURL("https://www.abc.com")));
}

// Test that we allow BNPL for Zip on an allowlisted URL.
TEST_F(AutofillOptimizationGuideTest,
       IsEligibleForBuyNowPayLater_ZipUrlAllowed) {
  // Ensure that `IsEligibleForBuyNowPayLater()` returns the right
  // response.
  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(GURL("https://www.abercrombie.com")),
              testing::Eq(
                  optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  // abercrombie.com is in the allowlist.
  EXPECT_TRUE(autofill_optimization_guide_->IsEligibleForBuyNowPayLater(
      /*issuer_id=*/"zip", GURL("https://www.abercrombie.com")));
}

// Test that we do not allow BNPL for Zip on a non-allowlisted URL.
TEST_F(AutofillOptimizationGuideTest,
       IsEligibleForBuyNowPayLater_ZipUrlBlocked) {
  // Ensure that `IsEligibleForBuyNowPayLater()` returns the right
  // response.
  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(GURL("https://www.abc.com")),
              testing::Eq(
                  optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  // abc.com is not in the allowlist.
  EXPECT_FALSE(autofill_optimization_guide_->IsEligibleForBuyNowPayLater(
      /*issuer_id=*/"zip", GURL("https://www.abc.com")));
}

// Test that we do not allow BNPL for unknown issuer id.
TEST_F(AutofillOptimizationGuideTest,
       IsEligibleForBuyNowPayLater_UnknownIssuerIdBlocked) {
  // Ensure that `IsEligibleForBuyNowPayLater()` returns the right
  // response.
  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(GURL("https://www.abercrombie.com")),
              testing::Eq(
                  optimization_guide::proto::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  // abercrombie.com is in the allowlist but issuer_id is not matched.
  EXPECT_FALSE(autofill_optimization_guide_->IsEligibleForBuyNowPayLater(
      /*issuer_id=*/"zipp", GURL("https://www.abercrombie.com")));
}
#endif

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
  EXPECT_CALL(*decider_,
              RegisterOptimizationTypes(testing::IsSupersetOf(
                  {optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST2,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST3,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST4,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST5,
                   optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST6})));
  autofill_optimization_guide_->OnDidParseForm(form_structure,
                                               personal_data_manager_.get());

  // Ensure that `IsEligibleForAblation()` returns the right responses.
  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::_, testing::_,
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));
  ON_CALL(
      *decider_,
      CanApplyOptimization(
          testing::Eq(GURL("https://www.example.com")),
          testing::Eq(optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));
  EXPECT_CALL(*decider_,
              CanApplyOptimization(
                  testing::_, testing::_,
                  testing::Matcher<optimization_guide::OptimizationMetadata*>(
                      testing::Eq(nullptr))))
      .Times(3);
  EXPECT_TRUE(autofill_optimization_guide_->IsEligibleForAblation(
      GURL("https://www.example.com"),
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1));
  // www.othersite.com is not on any list.
  EXPECT_FALSE(autofill_optimization_guide_->IsEligibleForAblation(
      GURL("https://www.othersite.com"),
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST1));
  // www.example.com is not on list 2, but on list 1.
  EXPECT_FALSE(autofill_optimization_guide_->IsEligibleForAblation(
      GURL("https://www.example.com"),
      optimization_guide::proto::AUTOFILL_ABLATION_SITES_LIST2));
}

struct BenefitOptimizationToBenefitCategoryTestCase {
  const std::string issuer_id;
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
    card_.set_issuer_id(GetParam().issuer_id);
    personal_data_manager_->test_payments_data_manager().AddServerCreditCard(
        card_);
  }

 private:
  CreditCard card_;
};

// Tests that the correct benefit category is returned when a benefit
// optimization is found for a particular credit card issuer and url.
TEST_P(BenefitOptimizationToBenefitCategoryTest,
       GetBenefitCategoryForOptimizationType) {
  GURL url = GURL("https://example.com/");
  ON_CALL(*decider_,
          CanApplyOptimization(
              testing::Eq(url), testing::Eq(expected_benefit_optimization()),
              testing::Matcher<optimization_guide::OptimizationMetadata*>(
                  testing::Eq(nullptr))))
      .WillByDefault(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_EQ(autofill_optimization_guide_
                ->AttemptToGetEligibleCreditCardBenefitCategory(
                    credit_card().issuer_id(), url),
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
            "capitalone",
            optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_DINING_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kDining},
        BenefitOptimizationToBenefitCategoryTestCase{
            "capitalone",
            optimization_guide::proto::CAPITAL_ONE_CREDIT_CARD_GROCERY_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kGroceryStores},
        BenefitOptimizationToBenefitCategoryTestCase{
            "capitalone",
            optimization_guide::proto::
                CAPITAL_ONE_CREDIT_CARD_ENTERTAINMENT_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kEntertainment},
        BenefitOptimizationToBenefitCategoryTestCase{
            "capitalone",
            optimization_guide::proto::
                CAPITAL_ONE_CREDIT_CARD_STREAMING_BENEFITS,
            CreditCardCategoryBenefit::BenefitCategory::kStreaming}));

}  // namespace autofill
