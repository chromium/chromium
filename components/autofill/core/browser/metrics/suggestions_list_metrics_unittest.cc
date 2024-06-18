// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

class SuggestionsListMetricsTest
    : public autofill_metrics::AutofillMetricsBaseTest,
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
  autofill_manager().OnFormsSeen({form}, {});
  personal_data().address_data_manager().AddProfile(test::GetFullProfile());
  personal_data().address_data_manager().AddProfile(test::GetFullProfile2());
  personal_data().payments_data_manager().AddCreditCard(test::GetCreditCard());
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    // There are 3 suggestions: 2 address profiles and one "manage addresses"
    // suggestion.
    histogram_tester.ExpectUniqueSample("Autofill.SuggestionsCount.Address", 3,
                                        1);
  }
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    // There are 2 suggestions: 1 card and one "manage payment methods"
    // suggestion.
    histogram_tester.ExpectUniqueSample("Autofill.SuggestionsCount.CreditCard",
                                        2, 1);
  }
}

// Test that we log the index of the accepted Autofill suggestions of the popup.
TEST_F(SuggestionsListMetricsTest, AcceptedSuggestionIndex) {
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FULL, .autocomplete_attribute = "name"},
                  {.role = EMAIL_ADDRESS, .autocomplete_attribute = "email"},
                  {.role = CREDIT_CARD_NUMBER,
                   .autocomplete_attribute = "cc-number"}}});
  autofill_manager().OnFormsSeen({form}, {});
  {
    Suggestion address_suggestion;
    address_suggestion.type = SuggestionType::kAddressEntry;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(address_suggestion, {1, 0});
    histogram_tester.ExpectUniqueSample(
        "Autofill.SuggestionAcceptedIndex.Profile", 1, 1);
  }
  {
    Suggestion credit_card_suggestion;
    credit_card_suggestion.type = SuggestionType::kCreditCardEntry;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(credit_card_suggestion, {0, 0});
    histogram_tester.ExpectUniqueSample(
        "Autofill.SuggestionAcceptedIndex.CreditCard", 0, 1);
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
  autofill_manager().OnFormsSeen({form}, {});
  {
    Suggestion address_suggestion;
    address_suggestion.type = SuggestionType::kAddressEntry;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().front().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(address_suggestion,
                                            /*position=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Suggestion.AcceptanceFieldValueLength.Address", 3, 1);
  }
  {
    Suggestion credit_card_suggestion;
    credit_card_suggestion.type = SuggestionType::kCreditCardEntry;
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields().back().global_id());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(credit_card_suggestion,
                                            /*position=*/{});
    histogram_tester.ExpectUniqueSample(
        "Autofill.Suggestion.AcceptanceFieldValueLength.CreditCard", 2, 1);
  }
}

}  // namespace

}  // namespace autofill::autofill_metrics
