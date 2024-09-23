// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class AutocompleteUnrecognizedFallbackEventLoggerTest
    : public AutofillMetricsBaseTest,
      public testing::Test {
 protected:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }

  // Show suggestions on the first field of the `form`.
  void ShowSuggestions(const FormData& form) {
    // On desktop, suggestion on an ac=unrecognized field can only be triggered
    // through source kManualFallbackAddress. However, on
    // mobile any trigger source works. For the test, it is irrelevant, since
    // the metric only cares about the autocomplete attribute, not the trigger
    // source.
    autofill_manager().OnAskForValuesToFillTest(
        form, form.fields()[0].global_id(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked);
    DidShowAutofillSuggestions(form);
  }
};

// Tests that when suggestion on an autocomplete=unrecognized field are shown,
// but not selected, the autocomplete=unrecognized FillAfterSuggestion metric is
// emitted correctly.
TEST_F(AutocompleteUnrecognizedFallbackEventLoggerTest,
       FillAfterSuggestion_NotFilled) {
  FormData form = test::CreateTestAddressFormData();
  test_api(form).field(0).set_parsed_autocomplete(
      AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized});
  SeeForm(form);
  ShowSuggestions(form);

  base::HistogramTester histogram_tester;
  // No suggestion was selected.
  ResetDriverToCommitMetrics();
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.ClassifiedFieldAutocompleteUnrecognized."
      "FillAfterSuggestion.Address",
      false, 1);
}

// Tests that when suggestion on an autocomplete=unrecognized field are shown
// and filled, the autocomplete=unrecognized FillAfterSuggestion metric is
// emitted correctly.
TEST_F(AutocompleteUnrecognizedFallbackEventLoggerTest,
       FillAfterSuggestion_Filled) {
  FormData form = test::CreateTestAddressFormData();
  test_api(form).field(0).set_parsed_autocomplete(
      AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized});
  ShowSuggestions(form);
  // Fill the suggestion.
  autofill_manager().FillOrPreviewProfileForm(
      mojom::ActionPersistence::kFill, form, form.fields()[0],
      *personal_data().address_data_manager().GetProfileByGUID(kTestProfileId),
      {.trigger_source = AutofillTriggerSource::kPopup});

  base::HistogramTester histogram_tester;
  ResetDriverToCommitMetrics();
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.ClassifiedFieldAutocompleteUnrecognized."
      "FillAfterSuggestion.Address",
      true, 1);
}

// Tests that the FillAfterSuggestion metric is not emitted when the form
// dynamically changes autocomplete attribute(s) before filling.
// Regression test for crbug.com/1483883.
TEST_F(AutocompleteUnrecognizedFallbackEventLoggerTest,
       FillAfterSuggestion_DynamicChange) {
  FormData form = test::CreateTestAddressFormData();
  SeeForm(form);
  // Since the form doesn't have any ac=unrecognized fields, the
  // `AutocompleteUnrecognizedFallbackEventLogger` is not notified.
  ShowSuggestions(form);

  // Dynamically change the autocomplete attribute before accepting the
  // suggestion. This causes `OnDidFillFormFillingSuggestion()` to be called,
  // even though `OnDidShowSuggestions()` was never called.
  test_api(form).field(0).set_parsed_autocomplete(
      AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized});
  SeeForm(form);
  autofill_manager().FillOrPreviewProfileForm(
      mojom::ActionPersistence::kFill, form, form.fields()[0],
      *personal_data().address_data_manager().GetProfileByGUID(kTestProfileId),
      {.trigger_source = AutofillTriggerSource::kPopup});

  base::HistogramTester histogram_tester;
  ResetDriverToCommitMetrics();
  histogram_tester.ExpectTotalCount(
      "Autofill.Funnel.ClassifiedFieldAutocompleteUnrecognized."
      "FillAfterSuggestion.Address",
      0);
}

// Tests that when suggestion on an non-autocomplete=unrecognized field are
// shown, the autocomplete=unrecognized acceptance FillAfterSuggestion is not
// emitted.
TEST_F(AutocompleteUnrecognizedFallbackEventLoggerTest,
       FillAfterSuggestion_RegularAutofill) {
  FormData form = test::CreateTestAddressFormData();
  SeeForm(form);
  ShowSuggestions(form);

  base::HistogramTester histogram_tester;
  ResetDriverToCommitMetrics();
  histogram_tester.ExpectTotalCount(
      "Autofill.Funnel.ClassifiedFieldAutocompleteUnrecognized."
      "FillAfterSuggestion.Address",
      0);
}

}  // namespace autofill::autofill_metrics
