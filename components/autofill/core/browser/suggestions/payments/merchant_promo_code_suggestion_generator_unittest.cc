// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/merchant_promo_code_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillNewSuggestionGeneration,
                              features::kAutofillEnableWalletDirectOffers},
        /*disabled_features=*/{});
    FormData form_data;
    form_data.set_fields(
        {test::CreateTestFormField(/*label=*/"", "Some Field Name",
                                   "SomePrefix", FormControlType::kInputText)});
    form_data.set_main_frame_origin(
        url::Origin::Create(GURL("https://www.example.com")));
    autofill_client_.set_last_committed_primary_main_frame_url(
        GURL("https://www.example.com"));
    form_structure_ = std::make_unique<FormStructure>(form_data);
    test_api(form()).SetFieldTypes({MERCHANT_PROMO_CODE});
    autofill_field_ = form_structure_->field(0);
    payments_data_manager().SetAutofillWalletImportEnabled(true);
    payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
  }

  TestAutofillClient& client() { return autofill_client_; }
  AutofillField& field() { return *autofill_field_; }
  FormStructure& form() { return *form_structure_; }
  TestPaymentsDataManager& payments_data_manager() {
    return autofill_client_.GetPersonalDataManager()
        .test_payments_data_manager();
  }

  std::vector<Suggestion> GetPromoCodeSuggestions() {
    MerchantPromoCodeSuggestionGenerator generator;
    std::vector<Suggestion> suggestions;

    auto on_suggestions_generated =
        [&suggestions](
            SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
          suggestions = returned_suggestions.second;
        };

    generator.GenerateSuggestions(form().ToFormData(), field(), &form(),
                                  &field(), client(), on_suggestions_generated);
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
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableWalletDirectOffers);

  DisplayStrings display_strings;
  display_strings.value_prop_text = "5% off (€10 max)";
  AutofillOfferData wallet_direct_offer = AutofillOfferData::WalletDirectOffer(
      /*offer_id=*/"2", base::Time::Now() + base::Days(2),
      {GURL("https://www.example.com")},
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, "test_promo_code_1");
  test_api(payments_data_manager())
      .AddOfferData(std::make_unique<AutofillOfferData>(wallet_direct_offer));

  Suggestion promo_code_suggestion =
      Suggestion(u"5% off (€10 max)", SuggestionType::kMerchantPromoCodeEntry);
  Suggestion separator_suggestion = Suggestion(SuggestionType::kSeparator);
  Suggestion footer_suggestion = Suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_OFFERS_FOOTER_TEXT),
      SuggestionType::kManageOffers);

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  MerchantPromoCodeSuggestionGenerator generator;

  EXPECT_CALL(
      suggestions_generated_callback,
      Run(testing::Pair(
          SuggestionGenerator::SuggestionDataSource::kMerchantPromoCode,
          UnorderedElementsAre(
              Field(&Suggestion::main_text, promo_code_suggestion.main_text),
              Field(&Suggestion::type, SuggestionType::kSeparator),
              Field(&Suggestion::type, footer_suggestion.type)))));
  generator.GenerateSuggestions(form().ToFormData(), field(), &form(), &field(),
                                client(), suggestions_generated_callback.Get());
}

// Checks that wallet direct offers do not generate suggestions when the promo
// code is already entered in the field.
TEST_F(MerchantPromoCodeSuggestionGeneratorTest,
       GeneratesPromoCodeSuggestions_FilledPromoCode_ReturnsNoSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableWalletDirectOffers);

  DisplayStrings display_strings;
  display_strings.value_prop_text = "5% off (€10 max)";
  AutofillOfferData wallet_direct_offer = AutofillOfferData::WalletDirectOffer(
      /*offer_id=*/"2", base::Time::Now() + base::Days(2),
      {GURL("https://www.example.com")},
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, "test_promo_code_1");
  test_api(payments_data_manager())
      .AddOfferData(std::make_unique<AutofillOfferData>(wallet_direct_offer));

  field().set_value(u"test_promo_code_1");

  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  MerchantPromoCodeSuggestionGenerator generator;

  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(
                  SuggestionGenerator::SuggestionDataSource::kMerchantPromoCode,
                  testing::IsEmpty())));
  generator.GenerateSuggestions(form().ToFormData(), field(), &form(), &field(),
                                client(), suggestions_generated_callback.Get());
}

TEST_F(MerchantPromoCodeSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_ValidPromoCodes) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableWalletDirectOffers);
  base::Time expiry = base::Time::Now() + base::Days(2);
  std::vector<GURL> merchant_origins{GURL("https://www.example.com")};

  DisplayStrings display_strings;
  display_strings.value_prop_text = "test_value_prop_text_1";
  std::string promo_code = "test_promo_code_1";
  AutofillOfferData offer1 = AutofillOfferData::WalletDirectOffer(
      /*offer_id=*/"1", expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, promo_code);

  DisplayStrings display_strings2;
  display_strings2.value_prop_text = "test_value_prop_text_2";
  std::string promo_code2 = "test_promo_code_2";
  AutofillOfferData offer2 = AutofillOfferData::WalletDirectOffer(
      /*offer_id=*/"2", expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings2, promo_code2);

  test_api(payments_data_manager())
      .AddOfferData(std::make_unique<AutofillOfferData>(offer1));
  test_api(payments_data_manager())
      .AddOfferData(std::make_unique<AutofillOfferData>(offer2));

  std::vector<Suggestion> promo_code_suggestions = GetPromoCodeSuggestions();

  ASSERT_EQ(promo_code_suggestions.size(), 4u);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value,
            u"test_value_prop_text_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::PromoCode>(),
            Suggestion::PromoCode("test_promo_code_1"));

  std::u16string expected_expiration_date = base::TimeFormatShortDate(expiry);
  EXPECT_THAT(promo_code_suggestions[0],
              Field(&Suggestion::labels,
                    std::vector<std::vector<Suggestion::Text>>{
                        {Suggestion::Text(l10n_util::GetStringFUTF16(
                            IDS_AUTOFILL_PROMO_CODE_SUGGESTION_CODE_LABEL,
                            u"test_promo_code_1"))},
                        {Suggestion::Text(l10n_util::GetStringFUTF16(
                            IDS_AUTOFILL_OFFERS_EXPIRES_ON,
                            expected_expiration_date))}}));

  EXPECT_EQ(promo_code_suggestions[0].icon, Suggestion::Icon::kOfferTag);
  EXPECT_EQ(promo_code_suggestions[0].type,
            SuggestionType::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[1].main_text.value,
            u"test_value_prop_text_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::PromoCode>(),
            Suggestion::PromoCode("test_promo_code_2"));
  EXPECT_THAT(promo_code_suggestions[1],
              Field(&Suggestion::labels,
                    std::vector<std::vector<Suggestion::Text>>{
                        {Suggestion::Text(l10n_util::GetStringFUTF16(
                            IDS_AUTOFILL_PROMO_CODE_SUGGESTION_CODE_LABEL,
                            u"test_promo_code_2"))},
                        {Suggestion::Text(l10n_util::GetStringFUTF16(
                            IDS_AUTOFILL_OFFERS_EXPIRES_ON,
                            expected_expiration_date))}}));
  EXPECT_EQ(promo_code_suggestions[1].icon, Suggestion::Icon::kOfferTag);
  EXPECT_EQ(promo_code_suggestions[1].type,
            SuggestionType::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[2].type, SuggestionType::kSeparator);

  EXPECT_EQ(promo_code_suggestions[3].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_OFFERS_FOOTER_TEXT));
  EXPECT_EQ(promo_code_suggestions[3].type, SuggestionType::kManageOffers);
  EXPECT_EQ(promo_code_suggestions[3].icon, Suggestion::Icon::kSettings);
}

// Checks that at most 5 promo code suggestions are generated, even if more
// promo code offers are available for the origin.
TEST_F(MerchantPromoCodeSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_LimitedToFiveSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableWalletDirectOffers);
  base::Time expiry = base::Time::Now() + base::Days(2);
  std::vector<GURL> merchant_origins{GURL("https://www.example.com")};

  // Add more offers than the maximum number of suggestions that can be shown.
  for (int i = 1; i <= 7; ++i) {
    std::string offer_id = base::NumberToString(i);
    DisplayStrings display_strings;
    display_strings.value_prop_text =
        base::StrCat({"test_value_prop_text_", offer_id});
    test_api(payments_data_manager())
        .AddOfferData(std::make_unique<AutofillOfferData>(
            AutofillOfferData::WalletDirectOffer(
                offer_id, expiry, merchant_origins,
                /*offer_details_url=*/GURL("https://offer-details-url.com/"),
                display_strings,
                base::StrCat({"test_promo_code_", offer_id}))));
  }

  std::vector<Suggestion> promo_code_suggestions = GetPromoCodeSuggestions();

  // 5 promo code suggestions, plus the separator and the footer.
  ASSERT_EQ(promo_code_suggestions.size(), 7u);

  // Only the first 5 offers are converted into suggestions.
  for (size_t i = 0; i < 5u; ++i) {
    SCOPED_TRACE(testing::Message() << "Suggestion index: " << i);
    EXPECT_EQ(promo_code_suggestions[i].type,
              SuggestionType::kMerchantPromoCodeEntry);
    EXPECT_EQ(promo_code_suggestions[i].main_text.value,
              base::UTF8ToUTF16(base::StrCat(
                  {"test_value_prop_text_", base::NumberToString(i + 1)})));
    EXPECT_EQ(promo_code_suggestions[i].GetPayload<Suggestion::PromoCode>(),
              Suggestion::PromoCode(base::StrCat(
                  {"test_promo_code_", base::NumberToString(i + 1)})));
  }

  EXPECT_EQ(promo_code_suggestions[5].type, SuggestionType::kSeparator);
  EXPECT_EQ(promo_code_suggestions[6].type, SuggestionType::kManageOffers);
}

}  // namespace
}  // namespace autofill
