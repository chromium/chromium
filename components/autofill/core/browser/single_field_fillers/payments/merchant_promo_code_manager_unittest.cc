// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"

#include <list>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using OnSuggestionsReturnedCallback =
    SingleFieldFillRouter::OnSuggestionsReturnedCallback;
using ::autofill::test::CreateTestFormField;
using ::testing::_;
using ::testing::Field;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

// Extends base::MockCallback to get references to the underlying callback.
//
// This is convenient because the functions we test take mutable references to
// the callbacks, which can't bind to the result of
// base::MockCallback<T>::Get().
class MockSuggestionsReturnedCallback
    : public base::MockCallback<OnSuggestionsReturnedCallback> {
 public:
  OnSuggestionsReturnedCallback& GetNewRef() {
    callbacks_.push_back(Get());
    return callbacks_.back();
  }

 private:
  using base::MockCallback<OnSuggestionsReturnedCallback>::Get;

  std::list<OnSuggestionsReturnedCallback> callbacks_;
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class MerchantPromoCodeManagerTest : public testing::Test {
 protected:
  MerchantPromoCodeManagerTest() {
    FormData form_data;
    form_data.set_fields(
        {CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                             FormControlType::kInputText)});
    form_data.set_main_frame_origin(
        url::Origin::Create(GURL("https://www.example.com")));
    form_structure_ = std::make_unique<FormStructure>(form_data);
    test_api(form()).SetFieldTypes({MERCHANT_PROMO_CODE});
    autofill_field_ = form_structure_->field(0);
  }

  // Sets up the TestPaymentsDataManager with a promo code offer for the given
  // `origin`, and sets the offer details url of the offer to
  // |offer_details_url`. Returns the promo code inserted in case the test wants
  // to match it against returned suggestions.
  std::string SetUpPromoCodeOffer(std::string origin,
                                  const GURL& offer_details_url) {
    payments_data_manager().SetAutofillWalletImportEnabled(true);
    payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
    AutofillOfferData testPromoCodeOfferData =
        test::GetPromoCodeOfferData(GURL(origin));
    testPromoCodeOfferData.SetOfferDetailsUrl(offer_details_url);
    test_api(payments_data_manager())
        .AddOfferData(
            std::make_unique<AutofillOfferData>(testPromoCodeOfferData));
    return testPromoCodeOfferData.GetPromoCode();
  }

  // Returns a mutable reference, which is valid until the next DoNothing()
  // call. This is needed because OnGetSingleFieldSuggestions() takes a mutable
  // reference to a non-null callback and consumes that callback.
  OnSuggestionsReturnedCallback& DoNothing() {
    do_nothing_ = base::DoNothing();
    return do_nothing_;
  }

  TestAutofillClient& client() { return autofill_client_; }
  AutofillField& field() { return *autofill_field_; }
  FormStructure& form() { return *form_structure_; }
  TestPaymentsDataManager& payments_data_manager() {
    return autofill_client_.GetPersonalDataManager()
        .test_payments_data_manager();
  }
  MerchantPromoCodeManager& promo_manager() {
    return merchant_promo_code_manager_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  MerchantPromoCodeManager merchant_promo_code_manager_{
      &payments_data_manager(),
      /*is_off_the_record=*/false};
  std::unique_ptr<FormStructure> form_structure_;
  // Owned by `form_structure_`.
  raw_ptr<AutofillField> autofill_field_ = nullptr;
  OnSuggestionsReturnedCallback do_nothing_ = base::DoNothing();
};

TEST_F(MerchantPromoCodeManagerTest, ShowsPromoCodeSuggestions) {
  std::string promo_code = SetUpPromoCodeOffer(
      "https://www.example.com", GURL("https://offer-details-url.com/"));
  Suggestion promo_code_suggestion = Suggestion(
      base::ASCIIToUTF16(promo_code), SuggestionType::kMerchantPromoCodeEntry);
  Suggestion footer_suggestion =
      Suggestion(l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT),
                 SuggestionType::kSeePromoCodeDetails);

  // Setting up mock to verify that the handler is returned a list of
  // promo-code-based suggestions and the promo code details line.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(
      mock_callback,
      Run(_, UnorderedElementsAre(
                 Field(&Suggestion::main_text, promo_code_suggestion.main_text),
                 Field(&Suggestion::type, SuggestionType::kSeparator),
                 Field(&Suggestion::main_text, footer_suggestion.main_text))))
      .Times(3);

  // Simulate request for suggestions.
  // Because all criteria are met, active promo code suggestions for the given
  // merchant site will be displayed instead of requesting Autocomplete
  // suggestions.
  EXPECT_TRUE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));

  // Trigger offers suggestions popup again to be able to test that we do not
  // log metrics twice for the same field.
  EXPECT_TRUE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));

  // Trigger offers suggestions popup again to be able to test that we log
  // metrics more than once if it is a different field.
  FormFieldData other_field =
      CreateTestFormField(/*label=*/"", "Some Other Name", "SomePrefix",
                          FormControlType::kInputTelephone);
  EXPECT_TRUE(promo_manager().OnGetSingleFieldSuggestions(
      form(), other_field, field(), client(), mock_callback.GetNewRef()));
}

TEST_F(MerchantPromoCodeManagerTest,
       DoesNotShowPromoCodeOffersIfFieldIsNotAPromoCodeField) {
  base::HistogramTester histogram_tester;
  // Setting up mock to verify that suggestions returning is not triggered if
  // the field is not a promo code field.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  field().SetTypeTo(AutofillType(UNKNOWN_TYPE),
                    AutofillPredictionSource::kHeuristics);
  // Simulate request for suggestions.
  EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));
}

TEST_F(MerchantPromoCodeManagerTest,
       DoesNotShowPromoCodeOffersForOffTheRecord) {
  std::string promo_code = SetUpPromoCodeOffer(
      "https://www.example.com", GURL("https://offer-details-url.com/"));
  client().set_is_off_the_record(true);

  // Setting up mock to verify that suggestions returning is not triggered if
  // the user is off the record.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));
}

TEST_F(MerchantPromoCodeManagerTest,
       DoesNotShowPromoCodeOffersIfPaymentsDataManagerDoesNotExist) {
  base::HistogramTester histogram_tester;
  promo_manager().payments_data_manager_ = nullptr;

  // Setting up mock to verify that suggestions returning is not triggered if
  // personal data manager does not exist.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));
}

TEST_F(MerchantPromoCodeManagerTest, NoPromoCodeOffers) {
  base::HistogramTester histogram_tester;

  // Setting up mock to verify that suggestions returning is not triggered if
  // there are no promo code offers to suggest.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));
}

// This test case exists to ensure that disabling autofill wallet import (by
// turning off the "Payment methods, offers, and addresses using Google Pay"
// toggle) disables offering suggestions and autofilling for promo codes.
TEST_F(MerchantPromoCodeManagerTest, AutofillWalletImportDisabled) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer("https://www.example.com",
                      GURL("https://offer-details-url.com/"));
  payments_data_manager().SetAutofillWalletImportEnabled(false);

  // Autofill wallet import is disabled, so check that we do not return
  // suggestions to the handler.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  {
    EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
        form(), field(), field(), client(), mock_callback.GetNewRef()));
  }
}

// This test case exists to ensure that disabling autofill credit card (by
// turning off the "Save and fill payment methods" toggle) disables offering
// suggestions and autofilling for promo codes.
TEST_F(MerchantPromoCodeManagerTest, AutofillCreditCardDisabled) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer("https://www.example.com",
                      GURL("https://offer-details-url.com/"));
  payments_data_manager().SetAutofillPaymentMethodsEnabled(false);

  // Autofill credit card is disabled, so check that we do not return
  // suggestions to the handler.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));
}

// This test case exists to ensure that we do not offer promo code offer
// suggestions if the field already contains a promo code.
TEST_F(MerchantPromoCodeManagerTest, PrefixMatched) {
  field().set_value(base::ASCIIToUTF16(SetUpPromoCodeOffer(
      "https://www.example.com", GURL("https://offer-details-url.com/"))));

  // The field contains the promo code already, so check that we do not return
  // suggestions to the handler.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));
}

}  // namespace autofill
