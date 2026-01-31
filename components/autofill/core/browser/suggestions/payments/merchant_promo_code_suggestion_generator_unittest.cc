// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"

#include "base/containers/to_vector.h"
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

  std::vector<Suggestion> GetPromoCodeSuggestionsFromPromoCodeOffers(
      const std::vector<const AutofillOfferData*>& promo_code_offers) {
    MerchantPromoCodeSuggestionGenerator generator;
    std::vector<Suggestion> suggestions;

    auto on_suggestions_generated =
        [&suggestions](
            SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
          suggestions = returned_suggestions.second;
        };

    std::vector<SuggestionGenerator::SuggestionData> suggestion_data =
        base::ToVector(std::move(promo_code_offers),
                       [](const AutofillOfferData* offer) {
                         return SuggestionGenerator::SuggestionData(*offer);
                       });
    base::flat_map<SuggestionGenerator::SuggestionDataSource,
                   std::vector<SuggestionGenerator::SuggestionData>>
        fetched_data = {
            {SuggestionGenerator::SuggestionDataSource::kMerchantPromoCode,
             std::move(suggestion_data)}};
    generator.GenerateSuggestions(form().ToFormData(), field(), &form(),
                                  &field(), client(), fetched_data,
                                  on_suggestions_generated);
    return suggestions;
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
  AutofillOfferData test_promo_code_offer_data =
      test::GetPromoCodeOfferData(GURL("https://www.example.com"));
  test_promo_code_offer_data.SetOfferDetailsUrl(
      GURL("https://offer-details-url.com/"));
  test_api(payments_data_manager())
      .AddOfferData(
          std::make_unique<AutofillOfferData>(test_promo_code_offer_data));
  std::string promo_code = test_promo_code_offer_data.GetPromoCode();

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
      saved_callback_argument;

  EXPECT_CALL(suggestion_data_callback,
              Run(testing::Pair(
                  SuggestionGenerator::SuggestionDataSource::kMerchantPromoCode,
                  testing::ElementsAre(test_promo_code_offer_data))))
      .WillOnce(testing::SaveArg<0>(&saved_callback_argument));
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
                                client(), {saved_callback_argument},
                                suggestions_generated_callback.Get());
}

TEST_F(MerchantPromoCodeSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_ValidPromoCodes) {
  std::vector<const AutofillOfferData*> promo_code_offers;

  base::Time expiry = base::Time::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  DisplayStrings display_strings;
  display_strings.value_prop_text = "test_value_prop_text_1";
  std::string promo_code = "test_promo_code_1";
  AutofillOfferData offer1 = AutofillOfferData::GPayPromoCodeOffer(
      /*offer_id=*/1, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, promo_code);

  promo_code_offers.push_back(&offer1);

  DisplayStrings display_strings2;
  display_strings2.value_prop_text = "test_value_prop_text_2";
  std::string promo_code2 = "test_promo_code_2";
  AutofillOfferData offer2 = AutofillOfferData::GPayPromoCodeOffer(
      /*offer_id=*/2, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings2, promo_code2);

  promo_code_offers.push_back(&offer2);

  std::vector<Suggestion> promo_code_suggestions =
      GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 4);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("1"));
  EXPECT_THAT(promo_code_suggestions[0],
              Field(&Suggestion::labels,
                    std::vector<std::vector<Suggestion::Text>>{
                        {Suggestion::Text(u"test_value_prop_text_1")}}));
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("1"));
  EXPECT_EQ(promo_code_suggestions[0].type,
            SuggestionType::kMerchantPromoCodeEntry);
  EXPECT_EQ(promo_code_suggestions[1].main_text.value, u"test_promo_code_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("2"));
  EXPECT_THAT(promo_code_suggestions[1],
              Field(&Suggestion::labels,
                    std::vector<std::vector<Suggestion::Text>>{
                        {Suggestion::Text(u"test_value_prop_text_2")}}));
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("2"));
  EXPECT_EQ(promo_code_suggestions[1].type,
            SuggestionType::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[2].type, SuggestionType::kSeparator);

  EXPECT_EQ(promo_code_suggestions[3].main_text.value,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));
  EXPECT_EQ(promo_code_suggestions[3].GetPayload<GURL>(),
            offer1.GetOfferDetailsUrl().spec());
  EXPECT_EQ(promo_code_suggestions[3].type,
            SuggestionType::kSeePromoCodeDetails);
}

TEST_F(MerchantPromoCodeSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_InvalidPromoCodeURL) {
  std::vector<const AutofillOfferData*> promo_code_offers;
  AutofillOfferData offer;
  offer.SetPromoCode("test_promo_code_1");
  offer.SetValuePropTextInDisplayStrings("test_value_prop_text_1");
  offer.SetOfferIdForTesting(1);
  offer.SetOfferDetailsUrl(GURL("invalid-url"));
  promo_code_offers.push_back(&offer);

  std::vector<Suggestion> promo_code_suggestions =
      GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 1);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  EXPECT_THAT(promo_code_suggestions[0],
              Field(&Suggestion::labels,
                    std::vector<std::vector<Suggestion::Text>>{
                        {Suggestion::Text(u"test_value_prop_text_1")}}));
  EXPECT_FALSE(std::holds_alternative<GURL>(promo_code_suggestions[0].payload));
  EXPECT_EQ(promo_code_suggestions[0].type,
            SuggestionType::kMerchantPromoCodeEntry);
}

}  // namespace
}  // namespace autofill
