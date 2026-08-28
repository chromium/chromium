// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

using test::CreateAutofillSuggestion;

class SuggestionsListMetricsTest : public AutofillMetricsBaseTest,
                                   public testing::Test {
 public:
  void SetUp() override {
    SetUpHelper();
    personal_data().test_address_data_manager().ClearProfiles();
    personal_data().test_payments_data_manager().ClearCreditCards();
  }
  void TearDown() override { TearDownHelper(); }
};

// Test that we log the number of Autofill suggestions when showing the popup.
TEST_F(SuggestionsListMetricsTest, SuggestionsCount) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
                  {.role = CREDIT_CARD_NUMBER,
                   .autocomplete_attribute = "cc-number"}}});
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  personal_data().address_data_manager().AddProfile(test::GetFullProfile());
  personal_data().address_data_manager().AddProfile(test::GetFullProfile2());
  personal_data().payments_data_manager().AddCreditCard(test::GetCreditCard());
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    // There are 3 suggestions: 2 address profiles and one "manage addresses"
    // suggestion. Manage suggestions are ignored.
    histogram_tester.ExpectUniqueSample("Autofill.SuggestionsCount.Address", 2,
                                        1);
  }
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    // There are 2 suggestions: 1 card and one "manage payment methods"
    // suggestion. Manage suggestions are ignored.
    histogram_tester.ExpectUniqueSample("Autofill.SuggestionsCount.CreditCard",
                                        1, 1);
  }
}

// Test that we log the index of the accepted Autofill suggestions of the popup.
TEST_F(SuggestionsListMetricsTest, AcceptedSuggestionIndex) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
                  {.role = CREDIT_CARD_NUMBER,
                   .autocomplete_attribute = "cc-number"}}});
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  {
    Suggestion address_suggestion(SuggestionType::kAddressEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(address_suggestion,
                                            {.multi_index = {1}});
    histogram_tester.ExpectUniqueSample(
        "Autofill.SuggestionAcceptedIndex.Address", 1, 1);
  }
  {
    Suggestion credit_card_suggestion(SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(credit_card_suggestion,
                                            {.multi_index = {0}});
    histogram_tester.ExpectUniqueSample(
        "Autofill.SuggestionAcceptedIndex.CreditCard", 0, 1);
  }
}

// Tests that the selected suggestion index is counted correctly if there are
// many available suggestions.
TEST_F(SuggestionsListMetricsTest, AcceptedSuggestionIndexDisplayedAtLeast) {
  const FormData form =
      test::GetFormData({.fields = {{.role = NAME_FULL},
                                    {.role = ADDRESS_HOME_STREET_ADDRESS},
                                    {.role = ADDRESS_HOME_CITY}}});
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  const FormFieldData& form_field = form.fields()[0];

  std::vector<Suggestion> suggestions(
      3, Suggestion(u"test", SuggestionType::kAddressEntry));
  {
    autofill_manager().OnAskForValuesToFillTest(form, form_field.global_id());
    external_delegate().OnSuggestionsReturned(form_field, suggestions,
                                              /*prefilled_query=*/{});

    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(suggestions[1],
                                            {.multi_index = {1}});
    histogram_tester.ExpectTotalCount(
        "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast5.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast10.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast20.Address", 0);
  }

  suggestions.resize(7, Suggestion(u"test", SuggestionType::kAddressEntry));
  {
    autofill_manager().OnAskForValuesToFillTest(form, form_field.global_id());
    external_delegate().OnSuggestionsReturned(form_field, suggestions,
                                              /*prefilled_query=*/{});

    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(suggestions[1],
                                            {.multi_index = {1}});
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast5.Address"),
        base::BucketsAre(base::Bucket(1, 1)));
    histogram_tester.ExpectTotalCount(
        "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast10.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast20.Address", 0);
  }

  suggestions.resize(10, Suggestion(u"test", SuggestionType::kAddressEntry));
  {
    autofill_manager().OnAskForValuesToFillTest(form, form_field.global_id());
    external_delegate().OnSuggestionsReturned(form_field, suggestions,
                                              /*prefilled_query=*/{});

    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(suggestions[5],
                                            {.multi_index = {5}});
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast5.Address"),
        base::BucketsAre(base::Bucket(5, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast10.Address"),
        base::BucketsAre(base::Bucket(5, 1)));
    histogram_tester.ExpectTotalCount(
        "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast20.Address", 0);
  }

  suggestions.resize(20, Suggestion(u"test", SuggestionType::kAddressEntry));
  {
    autofill_manager().OnAskForValuesToFillTest(form, form_field.global_id());
    external_delegate().OnSuggestionsReturned(form_field, suggestions,
                                              /*prefilled_query=*/{});

    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(suggestions[18],
                                            {.multi_index = {18}});
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast5.Address"),
        base::BucketsAre(base::Bucket(18, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast10.Address"),
        base::BucketsAre(base::Bucket(18, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast20.Address"),
        base::BucketsAre(base::Bucket(18, 1)));
  }
}

// Test that we log the length of the field's value right before accepting a
// suggestion.
TEST_F(SuggestionsListMetricsTest, AcceptanceFieldValueLength) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
                  {.role = CREDIT_CARD_NUMBER,
                   .autocomplete_attribute = "cc-number"}}});
  test_api(form).field(0).set_value(std::u16string(3, 'a'));
  test_api(form).field(-1).set_value(std::u16string(2, 'a'));
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  {
    Suggestion address_suggestion(SuggestionType::kAddressEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(address_suggestion,
                                            {.multi_index = {0}});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Suggestion.AcceptanceFieldValueLength.Address", 3, 1);
  }
  {
    Suggestion credit_card_suggestion(SuggestionType::kCreditCardEntry);
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(credit_card_suggestion,
                                            {.multi_index = {0}});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Suggestion.AcceptanceFieldValueLength.CreditCard", 2, 1);
  }
}

// Tests that `Autofill.AcceptedEmailSuggestion.Status` logs the correct
// type if only Autocomplete suggestions were shown and the user selected one.
TEST_F(SuggestionsListMetricsTest,
       LogMergedEmailAcceptedSuggestion_AutocompleteOnly) {
  const FormData form = test::GetFormData(
      {.fields = {
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
       }});
  const FormFieldData& field = form.fields()[0];
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  autofill_manager().OnAskForValuesToFillTest(form, field.global_id());

  external_delegate().OnSuggestionsReturned(
      field,
      {CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                u"user@example.com")},
      /*prefilled_query=*/{});

  base::HistogramTester histogram_tester;
  external_delegate().DidAcceptSuggestion(
      CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                               u"user@example.com"),
      {.multi_index = {0}});

  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedEmailSuggestion.Status",
      autofill_metrics::EmailSuggestionAcceptedStatus::kAutocompleteOnly, 1);
}

// Tests that `Autofill.AcceptedEmailSuggestion.Status` logs the correct
// type if only Address suggestions were shown and the user selected one.
TEST_F(SuggestionsListMetricsTest,
       LogMergedEmailAcceptedSuggestion_AddressOnly) {
  const FormData form = test::GetFormData(
      {.fields = {
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
       }});
  const FormFieldData& field = form.fields()[0];
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  autofill_manager().OnAskForValuesToFillTest(form, field.global_id());

  external_delegate().OnSuggestionsReturned(
      field,
      {CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                                u"user@example.com")},
      /*prefilled_query=*/{});

  base::HistogramTester histogram_tester;
  external_delegate().DidAcceptSuggestion(
      CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                               u"user@example.com"),
      {.multi_index = {0}});

  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedEmailSuggestion.Status",
      autofill_metrics::EmailSuggestionAcceptedStatus::kAddressOnly, 1);
}

// Tests that `Autofill.AcceptedEmailSuggestion.Status` logs the correct
// type if both Address and Autocomplete suggestions were shown and the user
// selected an autocomplete suggestion.
TEST_F(SuggestionsListMetricsTest,
       LogMergedEmailAcceptedSuggestion_MixedAutocompleteSelected) {
  const FormData form = test::GetFormData(
      {.fields = {
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
       }});
  const FormFieldData& field = form.fields()[0];
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  autofill_manager().OnAskForValuesToFillTest(form, field.global_id());

  external_delegate().OnSuggestionsReturned(
      field,
      {CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                                u"address@example.com"),
       CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                u"auto@example.com")},
      /*prefilled_query=*/{});

  base::HistogramTester histogram_tester;
  external_delegate().DidAcceptSuggestion(
      CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                               u"auto@example.com"),
      {.multi_index = {1}});

  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedEmailSuggestion.Status",
      autofill_metrics::EmailSuggestionAcceptedStatus::
          kMixedAutocompleteSelected,
      1);
}

// Tests that `Autofill.AcceptedEmailSuggestion.Status` logs the correct
// type if both Address and Autocomplete suggestions were shown and the user
// selected an address suggestion.
TEST_F(SuggestionsListMetricsTest,
       LogMergedEmailAcceptedSuggestion_MixedAddressSelected) {
  const FormData form = test::GetFormData(
      {.fields = {
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
       }});
  const FormFieldData& field = form.fields()[0];
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  autofill_manager().OnAskForValuesToFillTest(form, field.global_id());

  external_delegate().OnSuggestionsReturned(
      field,
      {CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                                u"address@example.com"),
       CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                u"auto@example.com")},
      /*prefilled_query=*/{});

  base::HistogramTester histogram_tester;
  external_delegate().DidAcceptSuggestion(
      CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                               u"address@example.com"),
      {.multi_index = {0}});

  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedEmailSuggestion.Status",
      autofill_metrics::EmailSuggestionAcceptedStatus::kMixedAddressSelected,
      1);
}

// Tests that `Autofill.AcceptedEmailSuggestion.Status` is not logged
// for non-email fields.
TEST_F(SuggestionsListMetricsTest,
       LogMergedEmailAcceptedSuggestion_NonEmailField) {
  const FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "name"},
           {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
       }});
  const FormFieldData& field = form.fields()[0];
  autofill_manager().OnFormsSeen({form}, {},
                                 AutofillManagerTestApi::pass_key());
  autofill_manager().OnAskForValuesToFillTest(form, field.global_id());

  external_delegate().OnSuggestionsReturned(
      field,
      {CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                                u"address@example.com"),
       CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                u"auto@example.com")},
      /*prefilled_query=*/{});

  base::HistogramTester histogram_tester;
  external_delegate().DidAcceptSuggestion(
      CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                               u"address@example.com"),
      {.multi_index = {0}});

  histogram_tester.ExpectTotalCount("Autofill.AcceptedEmailSuggestion.Status",
                                    0);
}

// Tests that the `Autofill.EmailPopup.SuggestionCount*` metrics are correctly
// emitted when merging address and autocomplete email suggestions.
TEST_F(SuggestionsListMetricsTest, LogMergedEmailSuggestionCounts) {
  base::test::ScopedFeatureList feature_list(
      {features::kAutofillNewSuggestionGeneration,
       features::kAutofillMergeAddressAndAutocompleteEmailSuggestions});

  const FormData form = test::GetFormData(
      {.fields = {test::FieldDescription{
           .label = u"Email",
           .form_control_type = FormControlType::kInputText}}});
  autofill_manager().AddSeenForm(form, {EMAIL_ADDRESS});

  // 1 Address suggestion, 2 Autocomplete suggestions.
  const std::vector<SuggestionGenerator::ReturnedSuggestions> input = {
      {SuggestionGenerator::SuggestionDataSource::kAddress,
       {Suggestion(u"address@example.com", SuggestionType::kAddressEntry)}},
      {SuggestionGenerator::SuggestionDataSource::kAutocomplete,
       {test::CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                       u"auto1@example.com"),
        test::CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                       u"auto2@example.com")}}};

  base::HistogramTester histogram_tester;

  test_api(autofill_manager())
      .OnIndividualSuggestionsGenerated(
          form, form.fields()[0],
          AutofillSuggestionTriggerSource::kFormControlElementClicked,
          base::TimeTicks::Now(), input);

  histogram_tester.ExpectUniqueSample("Autofill.EmailPopup.SuggestionCount",
                                      /*sample=*/3,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.EmailPopup.SuggestionCount.Address", /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.EmailPopup.SuggestionCount.Autocomplete", /*sample=*/2,
      /*expected_bucket_count=*/1);
}

}  // namespace

}  // namespace autofill::autofill_metrics
