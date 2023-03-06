// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

class MockOptimizationGuideDecider
    : public optimization_guide::NewOptimizationGuideDecider {
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
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback),
      (override));
};

class AutofillOptimizationGuideTest : public testing::Test {
 public:
  void SetUp() override {
    decider_ = std::make_unique<MockOptimizationGuideDecider>();
    autofill_optimization_guide_ =
        std::make_unique<AutofillOptimizationGuide>(decider_.get());
  }

 protected:
  std::unique_ptr<AutofillOptimizationGuide> autofill_optimization_guide_;
  std::unique_ptr<MockOptimizationGuideDecider> decider_;
  test::AutofillEnvironment autofill_environment_;
};

TEST_F(AutofillOptimizationGuideTest, EnsureIntegratorInitializedCorrectly) {
  EXPECT_TRUE(autofill_optimization_guide_
                  ->GetOptimizationGuideKeyedServiceForTesting() ==
              decider_.get());
}

TEST_F(AutofillOptimizationGuideTest, IbanFieldFound) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableIbanClientSideUrlFiltering);
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  std::vector<optimization_guide::proto::OptimizationType> optimization_types =
      {optimization_guide::proto::IBAN_AUTOFILL_BLOCKED};
  EXPECT_CALL(*decider_, RegisterOptimizationTypes(testing::ElementsAre(
                             optimization_guide::proto::IBAN_AUTOFILL_BLOCKED)))
      .Times(1);

  autofill_optimization_guide_->OnDidParseForm(form_structure);
}

TEST_F(AutofillOptimizationGuideTest, IbanFieldFound_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnableIbanClientSideUrlFiltering);
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure);
}

TEST_F(AutofillOptimizationGuideTest, IbanFieldNotFound) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableIbanClientSideUrlFiltering);
  AutofillField field;
  FormData form_data;
  form_data.fields = {field};
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({MERCHANT_PROMO_CODE}, {MERCHANT_PROMO_CODE});

  EXPECT_CALL(*decider_, RegisterOptimizationTypes).Times(0);

  autofill_optimization_guide_->OnDidParseForm(form_structure);
}

TEST_F(AutofillOptimizationGuideTest, ShouldBlockSingleFieldSuggestions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableIbanClientSideUrlFiltering);
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});
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

TEST_F(AutofillOptimizationGuideTest,
       ShouldNotBlockSingleFieldSuggestions_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnableIbanClientSideUrlFiltering);
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});
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

TEST_F(AutofillOptimizationGuideTest, ShouldNotBlockSingleFieldSuggestions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableIbanClientSideUrlFiltering);
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});
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

TEST_F(AutofillOptimizationGuideTest,
       ShouldNotBlockSingleFieldSuggestions_FieldTypeForBlockingNotFound) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableIbanClientSideUrlFiltering);
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
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

}  // namespace autofill
