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
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/metrics/payments/promo_code_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

constexpr char kTestOriginUrl[] = "https://www.example.com";

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

// Returns a form that is classified as a credit card form.
FormData CreateCreditCardFormData() {
  return test::GetFormData(test::FormDescription{
      .fields = {{.role = CREDIT_CARD_NAME_FULL},
                 {.role = CREDIT_CARD_NUMBER},
                 {.role = CREDIT_CARD_EXP_MONTH},
                 {.role = CREDIT_CARD_EXP_4_DIGIT_YEAR}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
}

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class MerchantPromoCodeManagerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 protected:
  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();
    client().set_last_committed_primary_main_frame_url(GURL(kTestOriginUrl));
    merchant_promo_code_manager_ =
        std::make_unique<MerchantPromoCodeManager>(&autofill_client());
    FormData form_data;
    form_data.set_fields(
        {CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                             FormControlType::kInputText)});
    form_data.set_main_frame_origin(url::Origin::Create(GURL(kTestOriginUrl)));
    form_structure_ = std::make_unique<FormStructure>(form_data);
    test_api(form()).SetFieldTypes({MERCHANT_PROMO_CODE});
    autofill_field_ = form_structure_->field(0);
  }

  // Sets up the TestPaymentsDataManager with a promo code offer for the given
  // `origin`, and sets the offer details url of the offer to
  // `offer_details_url`. Returns the promo code inserted in case the test wants
  // to match it against returned suggestions.
  std::string SetUpPromoCodeOffer(
      std::string origin,
      const GURL& offer_details_url = GURL("https://offer-details-url.com/")) {
    payments_data_manager().SetAutofillWalletImportEnabled(true);
    payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
    AutofillOfferData test_promo_code_offer_data =
        test::GetPromoCodeOfferData(GURL(origin));
    test_promo_code_offer_data.SetOfferDetailsUrl(offer_details_url);
    test_api(payments_data_manager())
        .AddOfferData(
            std::make_unique<AutofillOfferData>(test_promo_code_offer_data));
    return test_promo_code_offer_data.GetPromoCode();
  }

  // Returns a mutable reference, which is valid until the next DoNothing()
  // call. This is needed because OnGetSingleFieldSuggestions() takes a mutable
  // reference to a non-null callback and consumes that callback.
  OnSuggestionsReturnedCallback& DoNothing() {
    do_nothing_ = base::DoNothing();
    return do_nothing_;
  }

  TestAutofillClient& client() { return autofill_client(); }
  AutofillField& field() { return *autofill_field_; }
  FormStructure& form() { return *form_structure_; }
  TestPaymentsDataManager& payments_data_manager() {
    return autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager();
  }
  MerchantPromoCodeManager& promo_manager() {
    return *merchant_promo_code_manager_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnableWalletDirectOffers};
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<MerchantPromoCodeManager> merchant_promo_code_manager_;
  std::unique_ptr<FormStructure> form_structure_;
  // Owned by `form_structure_`.
  raw_ptr<AutofillField> autofill_field_ = nullptr;
  OnSuggestionsReturnedCallback do_nothing_ = base::DoNothing();
};

TEST_F(MerchantPromoCodeManagerTest, ShowsPromoCodeSuggestions) {
  SetUpPromoCodeOffer(kTestOriginUrl, GURL("https://offer-details-url.com/"));
  Suggestion promo_code_suggestion =
      Suggestion(u"5% off on shoes.", SuggestionType::kMerchantPromoCodeEntry);

  // Setting up mock to verify that the handler is returned a list of
  // promo-code-based suggestions, followed by a separator and the "Manage
  // offers" footer.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(
      mock_callback,
      Run(_, UnorderedElementsAre(
                 Field(&Suggestion::main_text, promo_code_suggestion.main_text),
                 Field(&Suggestion::type, SuggestionType::kSeparator),
                 Field(&Suggestion::type, SuggestionType::kManageOffers))))
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
      kTestOriginUrl, GURL("https://offer-details-url.com/"));
  client().set_is_off_the_record(true);

  // Setting up mock to verify that suggestions returning is not triggered if
  // the user is off the record.
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
  SetUpPromoCodeOffer(kTestOriginUrl, GURL("https://offer-details-url.com/"));
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
  SetUpPromoCodeOffer(kTestOriginUrl, GURL("https://offer-details-url.com/"));
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
      kTestOriginUrl, GURL("https://offer-details-url.com/"))));

  // The field contains the promo code already, so check that we do not return
  // suggestions to the handler.
  MockSuggestionsReturnedCallback mock_callback;
  EXPECT_CALL(mock_callback, Run).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(promo_manager().OnGetSingleFieldSuggestions(
      form(), field(), field(), client(), mock_callback.GetNewRef()));
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_VisiblePromoCodeField_ShowIphBubble) {
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});

  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());

  EXPECT_TRUE(client().IsShowingWalletDirectOffersIph());
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_NoVisiblePromoCodeField_DoesNotShowIphBubble) {
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = false}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});

  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());

  EXPECT_FALSE(client().IsShowingWalletDirectOffersIph());
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_NoOffersForOrigin_DoesNotShowIphBubble) {
  FormData form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});

  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());

  EXPECT_FALSE(client().IsShowingWalletDirectOffersIph());
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_FeatureDisabled_DoesNotShowIphBubble) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableWalletDirectOffers);

  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});

  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());

  EXPECT_FALSE(client().IsShowingWalletDirectOffersIph());
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_NoOffer_LogsNavigatedWithoutPromoCode) {
  base::HistogramTester histogram_tester;

  FormData form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());

  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kNavigatedToPageWithoutPromoCode,
      1);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_NoPromoCodeField_DoesNotLog) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(form_data.global_id()));

  histogram_tester.ExpectTotalCount("Autofill.FormEvents.PromoCode", 0);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_InvisiblePromoCodeField_DoesNotLog) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = false}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(form_data.global_id()));

  histogram_tester.ExpectTotalCount("Autofill.FormEvents.PromoCode", 0);
}

// Tests that the page load event is logged at most once per page load, and is
// logged again after `Reset()` (i.e. after a navigation).
TEST_F(MerchantPromoCodeManagerTest,
       OnFieldTypesDetermined_LogsOncePerPageLoad_ResetAllowsLoggingAgain) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});

  // Seeing the promo code field twice on the same page only logs once.
  autofill_manager().OnFormsSeen({promo_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  autofill_manager().OnFormsSeen({promo_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kNavigatedToPageWithPromoCode, 1);

  // After a reset, seeing the promo code field logs a second time.
  promo_manager().Reset();
  autofill_manager().OnFormsSeen({promo_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kNavigatedToPageWithPromoCode, 2);
}

TEST_F(MerchantPromoCodeManagerTest, DidShowSuggestions_LogsOncePerPageLoad) {
  base::HistogramTester histogram_tester;

  // Calling DidShowSuggestions logs the funnel event.
  promo_manager().DidShowSuggestions();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionsShown, 1);

  // Calling DidShowSuggestions again on the same page does not log again.
  promo_manager().DidShowSuggestions();
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionsShown, 1);
}

TEST_F(MerchantPromoCodeManagerTest,
       DidShowSuggestions_Reset_AllowsSubsequentLogging) {
  base::HistogramTester histogram_tester;

  promo_manager().DidShowSuggestions();
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionsShown, 1);

  // Resetting page load metrics allows the event to be logged again on the next
  // show.
  promo_manager().Reset();
  promo_manager().DidShowSuggestions();
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionsShown, 2);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnSingleFieldSuggestionSelected_PromoSuggestion_LogsOncePerPageLoad) {
  base::HistogramTester histogram_tester;

  Suggestion promo_code_suggestion(SuggestionType::kMerchantPromoCodeEntry);

  // Selecting a promo code suggestion logs the filled funnel event.
  promo_manager().OnSingleFieldSuggestionSelected(promo_code_suggestion);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionFilled, 1);

  // Selecting a promo code suggestion again on the same page does not log
  // again.
  promo_manager().OnSingleFieldSuggestionSelected(promo_code_suggestion);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionFilled, 1);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnSingleFieldSuggestionSelected_Reset_AllowsSubsequentLogging) {
  base::HistogramTester histogram_tester;

  Suggestion promo_code_suggestion(SuggestionType::kMerchantPromoCodeEntry);

  promo_manager().OnSingleFieldSuggestionSelected(promo_code_suggestion);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionFilled, 1);

  // Resetting page load metrics allows the event to be logged again on the next
  // fill.
  promo_manager().Reset();
  promo_manager().OnSingleFieldSuggestionSelected(promo_code_suggestion);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kPromoCodeSuggestionFilled, 2);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_NoPromoCodeFieldOnPage_DoesNotLog) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(form_data.global_id()));

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), form_data);

  histogram_tester.ExpectTotalCount("Autofill.FormEvents.PromoCode", 0);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_PromoCodePresent_LogsWithPromoCodePresentForPage) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(cc_form_data.global_id()));

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      1);
  // No promo code suggestion was filled.
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kFormSubmittedAfterPromoCodeFilled,
      0);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_NoPromoCodePresent_LogsWithoutPromoCodePresentForPage) {
  base::HistogramTester histogram_tester;

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(cc_form_data.global_id()));

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithoutPromoCodePresentForPage,
      1);
}

TEST_F(
    MerchantPromoCodeManagerTest,
    OnFormSubmitted_PromoCodeAddedAfterPageLoad_LogsWithPromoCodePresentForPage) {
  base::HistogramTester histogram_tester;

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(cc_form_data.global_id()));

  SetUpPromoCodeOffer(kTestOriginUrl);

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      1);
}

// Tests that submission events are logged at most once per page load, and are
// logged again after `Reset()` (i.e. after a navigation).
TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_LogsOncePerPageLoad_ResetAllowsLoggingAgain) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(cc_form_data.global_id()));
  Suggestion promo_code_suggestion(SuggestionType::kMerchantPromoCodeEntry);

  // Submitting twice on the same page only logs once.
  promo_manager().OnSingleFieldSuggestionSelected(promo_code_suggestion);
  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);
  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kFormSubmittedAfterPromoCodeFilled,
      1);

  // After a reset, filling and submitting again logs a second time.
  promo_manager().Reset();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  promo_manager().OnSingleFieldSuggestionSelected(promo_code_suggestion);
  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      2);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kFormSubmittedAfterPromoCodeFilled,
      2);
}

// Tests that submitting a form that is not a credit card form, e.g. a form that
// only contains a promo code field, does not log.
TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_NonCreditCardForm_DoesNotLog) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  autofill_manager().OnFormsSeen({form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(form_data.global_id()));

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), form_data);

  // Only the page load event is logged, not any form submission events.
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kNavigatedToPageWithPromoCode, 1);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_PromoCodeFieldContainingValue_LogsMetric) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE,
                  .is_visible = true,
                  .value = u"DISCOUNT"}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(cc_form_data.global_id()));

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodeFieldContainingValue,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      1);
}

// Tests that a promo code that was entered and then deleted before submission
// is not counted as a value of the promo code field.
TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_PromoCodeFieldCleared_DoesNotLogContainingValue) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE,
                  .is_visible = true,
                  .value = u"PROMOCODE"}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(cc_form_data.global_id()));

  // The user deletes the promo code, which updates the cached form.
  test_api(promo_form_data).field(0).set_value(u"");
  autofill_manager().OnFormsSeen({promo_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodeFieldContainingValue,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      1);
}

// Tests that the submitted value is used for a promo code field in the
// submitted form, since the cached form does not contain it yet.
TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_PromoCodeFieldInSubmittedForm_UsesSubmittedValue) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData checkout_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = CREDIT_CARD_NAME_FULL},
                 {.role = CREDIT_CARD_NUMBER},
                 {.role = CREDIT_CARD_EXP_MONTH},
                 {.role = CREDIT_CARD_EXP_4_DIGIT_YEAR},
                 {.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  autofill_manager().OnFormsSeen({checkout_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(
      autofill_manager().FindCachedFormById(checkout_form_data.global_id()));

  test_api(checkout_form_data).field(-1).set_value(u"PROMOCODE");
  promo_manager().OnBeforeFormSubmitted(autofill_manager(), checkout_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodeFieldContainingValue,
      1);
}

// Tests that a promo code field in the submitted form that was cleared right
// before submission is not counted, even if the cached form still has a value.
TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_PromoCodeFieldClearedInSubmittedForm_DoesNotLog) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData checkout_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = CREDIT_CARD_NAME_FULL},
                 {.role = CREDIT_CARD_NUMBER},
                 {.role = CREDIT_CARD_EXP_MONTH},
                 {.role = CREDIT_CARD_EXP_4_DIGIT_YEAR},
                 {.role = MERCHANT_PROMO_CODE,
                  .is_visible = true,
                  .value = u"PROMOCODE"}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  autofill_manager().OnFormsSeen({checkout_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(
      autofill_manager().FindCachedFormById(checkout_form_data.global_id()));

  test_api(checkout_form_data).field(-1).set_value(u"");
  promo_manager().OnBeforeFormSubmitted(autofill_manager(), checkout_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodeFieldContainingValue,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      1);
}

// Tests that when a page has multiple promo code forms, every one is checked
// for a value on submission, not just the most recently parsed form, and that
// the containing value event is only logged once.
TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_MultiplePromoCodeForms_ChecksAllFormsAndLogsOnce) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData promo_form_data_1 = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE,
                  .is_visible = true,
                  .value = u"DISCOUNT1"}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData promo_form_data_2 = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE,
                  .is_visible = true,
                  .value = u"DISCOUNT2"}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData empty_promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();

  // Parse the empty promo code form last.
  autofill_manager().OnFormsSeen(
      {promo_form_data_1, promo_form_data_2, cc_form_data}, {},
      AutofillManagerTestApi::pass_key());
  autofill_manager().OnFormsSeen({empty_promo_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(
      autofill_manager().FindCachedFormById(promo_form_data_1.global_id()));
  ASSERT_TRUE(
      autofill_manager().FindCachedFormById(promo_form_data_2.global_id()));
  ASSERT_TRUE(
      autofill_manager().FindCachedFormById(empty_promo_form_data.global_id()));

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodeFieldContainingValue,
      1);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnFormSubmitted_AfterSuggestionFilled_LogsAfterPromoCodeFilled) {
  base::HistogramTester histogram_tester;
  SetUpPromoCodeOffer(kTestOriginUrl);

  FormData promo_form_data = test::GetFormData(test::FormDescription{
      .fields = {{.role = MERCHANT_PROMO_CODE, .is_visible = true}},
      .main_frame_origin = url::Origin::Create(GURL(kTestOriginUrl))});
  FormData cc_form_data = CreateCreditCardFormData();
  autofill_manager().OnFormsSeen({promo_form_data, cc_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  ASSERT_TRUE(autofill_manager().FindCachedFormById(cc_form_data.global_id()));

  // Filling the promo code updates the cached form with the filled value.
  test_api(promo_form_data).field(0).set_value(u"DISCOUNT");
  autofill_manager().OnFormsSeen({promo_form_data}, {},
                                 AutofillManagerTestApi::pass_key());
  Suggestion promo_code_suggestion(SuggestionType::kMerchantPromoCodeEntry);
  promo_manager().OnSingleFieldSuggestionSelected(promo_code_suggestion);

  promo_manager().OnBeforeFormSubmitted(autofill_manager(), cc_form_data);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::kFormSubmittedAfterPromoCodeFilled,
      1);
  // The filled promo code also counts as a value of the promo code field.
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodeFieldContainingValue,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.PromoCode",
      autofill_metrics::PromoCodeFormEvent::
          kFormSubmittedWithPromoCodePresentForPage,
      1);
}

}  // namespace autofill
