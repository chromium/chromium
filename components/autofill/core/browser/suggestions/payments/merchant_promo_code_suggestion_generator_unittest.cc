// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::Field;

class MerchantPromoCodeSuggestionGeneratorTest : public testing::Test {
 protected:
  MerchantPromoCodeSuggestionGeneratorTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillNewSuggestionGeneration);
    FormData form_data;
    form_data.set_fields(
        {test::CreateTestFormField(/*label=*/"", "Some Field Name",
                                   "SomePrefix", FormControlType::kInputText)});
    form_data.set_main_frame_origin(
        url::Origin::Create(GURL("https://www.example.com")));
    form_structure_ = std::make_unique<FormStructure>(form_data);
    test_api(form()).SetFieldTypes({MERCHANT_PROMO_CODE});
    autofill_field_ = form_structure_->field(0);
  }

  TestAutofillClient& client() { return autofill_client_; }
  AutofillField& field() { return *autofill_field_; }
  FormStructure& form() { return *form_structure_; }
  TestPaymentsDataManager& payments_data_manager() {
    return autofill_client_.GetPersonalDataManager()
        .test_payments_data_manager();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<FormStructure> form_structure_;
  // Owned by `form_structure_`.
  raw_ptr<AutofillField> autofill_field_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

// Checks that all merchant promo codes are returned as suggestion data, and
// used for generating suggestions.
TEST_F(MerchantPromoCodeSuggestionGeneratorTest,
       GeneratesPromoCodeSuggestions) {
  payments_data_manager().SetAutofillWalletImportEnabled(true);
  payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
  AutofillOfferData testPromoCodeOfferData =
      test::GetPromoCodeOfferData(GURL("https://www.example.com"));
  testPromoCodeOfferData.SetOfferDetailsUrl(
      GURL("https://offer-details-url.com/"));
  test_api(payments_data_manager())
      .AddOfferData(
          std::make_unique<AutofillOfferData>(testPromoCodeOfferData));
  std::string promo_code = testPromoCodeOfferData.GetPromoCode();

  Suggestion promo_code_suggestion = Suggestion(
      base::ASCIIToUTF16(promo_code), SuggestionType::kMerchantPromoCodeEntry);
  Suggestion separator_suggestion = Suggestion(SuggestionType::kSeparator);
  Suggestion footer_suggestion =
      Suggestion(l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT),
                 SuggestionType::kSeePromoCodeDetails);

  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  MerchantPromoCodeSuggestionGenerator generator;
  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      savedCallbackArgument;

  EXPECT_CALL(suggestion_data_callback,
              Run(testing::Pair(
                  SuggestionGenerator::SuggestionDataSource::kMerchantPromoCode,
                  testing::ElementsAre(testPromoCodeOfferData))))
      .WillOnce(testing::SaveArg<0>(&savedCallbackArgument));
  generator.FetchSuggestionData(form().ToFormData(), field(), &form(), &field(),
                                client(), suggestion_data_callback.Get());

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(testing::Pair(
          FillingProduct::kMerchantPromoCode,
          UnorderedElementsAre(
              Field(&Suggestion::main_text, promo_code_suggestion.main_text),
              Field(&Suggestion::type, SuggestionType::kSeparator),
              Field(&Suggestion::main_text, footer_suggestion.main_text)))));
  generator.GenerateSuggestions(form().ToFormData(), field(), &form(), &field(),
                                client(), {savedCallbackArgument},
                                suggestions_generated_callback.Get());
}

}  // namespace
}  // namespace autofill
