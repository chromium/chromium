// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

class SuggestionsListMetricsTest
    : public autofill_metrics::AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override {
    SetUpHelper();
    personal_data().ClearProfiles();
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
  personal_data().AddProfile(test::GetFullProfile());
  personal_data().AddProfile(test::GetFullProfile2());
  personal_data().AddCreditCard(test::GetCreditCard());
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.front());
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 2,
                                        1);
    histogram_tester.ExpectUniqueSample("Autofill.SuggestionsCount.Address", 2,
                                        1);
  }
  {
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
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
  autofill_manager().OnFormsSeen({form}, {});
  {
    Suggestion address_suggestion;
    address_suggestion.popup_item_id = PopupItemId::kAddressEntry;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.front());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(address_suggestion, {1, 0});
    histogram_tester.ExpectUniqueSample(
        "Autofill.SuggestionAcceptedIndex.Profile", 1, 1);
  }
  {
    Suggestion credit_card_suggestion;
    credit_card_suggestion.popup_item_id = PopupItemId::kCreditCardEntry;
    autofill_manager().OnAskForValuesToFillTest(form, form.fields.back());
    base::HistogramTester histogram_tester;
    external_delegate().DidAcceptSuggestion(credit_card_suggestion, {0, 0});
    histogram_tester.ExpectUniqueSample(
        "Autofill.SuggestionAcceptedIndex.CreditCard", 0, 1);
  }
}

}  // namespace

}  // namespace autofill::autofill_metrics
