// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autocomplete_key_metrics_logger.h"

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table_label_sensitive.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::Test;

Suggestion CreateAutocompleteSuggestion(const std::u16string& value,
                                        MatchingType matching_type) {
  Suggestion suggestion(value, SuggestionType::kAutocompleteEntry);
  suggestion.payload = AutocompleteSearchResultLabelSensitive(
      value, matching_type, /*query_name=*/u"name", /*query_label=*/u"label",
      /*count=*/1, /*date_last_used=*/base::Time::Now());
  return suggestion;
}

class AutocompleteKeyMetricsLoggerTest : public Test {
 protected:
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  AutocompleteKeyMetricsLogger& logger() { return logger_; }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillLabelSensitiveAutocomplete};
  base::HistogramTester histogram_tester_;
  AutocompleteKeyMetricsLogger logger_;
};

// Verifies that when autocomplete suggestions of various matching types are
// shown but none are accepted, FillingAcceptance records false across total and
// all shown types, while FillingCorrectness is not emitted.
TEST_F(AutocompleteKeyMetricsLoggerTest, SuggestionsShown_NotAccepted) {
  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  std::vector<Suggestion> suggestions = {
      CreateAutocompleteSuggestion(u"Val1", MatchingType::kName),
      CreateAutocompleteSuggestion(u"Val2", MatchingType::kLabel),
      CreateAutocompleteSuggestion(u"Val3", MatchingType::kNameAndLabel),
  };

  logger().OnSuggestionsShown(field_id, suggestions);
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance", false, 1);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingAcceptance.ByFieldType", 0);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Name", false, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Label", false, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.BothNameAndLabel", false, 1);

  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.Name", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.Label", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.BothNameAndLabel", 0);
}

// Verifies that when an autocomplete suggestion is accepted and submitted
// without manual editing, FillingAcceptance records true for the winning type
// and false for unselected types, and FillingCorrectness records true.
TEST_F(AutocompleteKeyMetricsLoggerTest, SuggestionAccepted_SubmittedUnedited) {
  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  std::vector<Suggestion> suggestions = {
      CreateAutocompleteSuggestion(u"John", MatchingType::kName),
      CreateAutocompleteSuggestion(u"Jane", MatchingType::kLabel),
  };

  logger().OnSuggestionsShown(field_id, suggestions);
  logger().OnSuggestionSelected(field_id, suggestions[0]);

  // Field was submitted with the exact filled value.
  test_api(form).field(0).set_value(u"John");
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Name", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Label", false, 1);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingAcceptance.BothNameAndLabel", 0);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness.Name", true, 1);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.Label", 0);
}

// Verifies that accepting a suggestion with MatchingType::kNameAndLabel
// emits the .BothNameAndLabel variant for both FillingAcceptance and
// FillingCorrectness, and that when multiple suggestions are accepted on the
// same field before submission, the last accepted suggestion is evaluated.
TEST_F(AutocompleteKeyMetricsLoggerTest,
       BothNameAndLabelAccepted_LastSelectionWins) {
  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  std::vector<Suggestion> suggestions = {
      CreateAutocompleteSuggestion(u"John", MatchingType::kName),
      CreateAutocompleteSuggestion(u"John Doe", MatchingType::kNameAndLabel),
  };

  logger().OnSuggestionsShown(field_id, suggestions);
  // First select the Name-only suggestion, then select the BothNameAndLabel
  // suggestion on the same field.
  logger().OnSuggestionSelected(field_id, suggestions[0]);
  logger().OnSuggestionSelected(field_id, suggestions[1]);

  test_api(form).field(0).set_value(u"John Doe");
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Name", false, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.BothNameAndLabel", true, 1);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingAcceptance.Label", 0);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness.BothNameAndLabel", true, 1);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.Name", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.Label", 0);
}

// Verifies that when an autocomplete suggestion is accepted but subsequently
// edited before form submission, FillingCorrectness records false.
TEST_F(AutocompleteKeyMetricsLoggerTest, SuggestionAccepted_SubmittedEdited) {
  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  std::vector<Suggestion> suggestions = {
      CreateAutocompleteSuggestion(u"Apt 4B", MatchingType::kLabel),
  };

  logger().OnSuggestionsShown(field_id, suggestions);
  logger().OnSuggestionSelected(field_id, suggestions[0]);

  // User edited the field to "Apt 4C" before submission.
  test_api(form).field(0).set_value(u"Apt 4C");
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Label", true, 1);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness", false, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness.Label", false, 1);
}

// Verifies that multiple suggestion showings for the same field do not
// inflate the sample count in FillingAcceptance.
TEST_F(AutocompleteKeyMetricsLoggerTest, MultipleFocusesDeduplicated) {
  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  std::vector<Suggestion> suggestions1 = {
      CreateAutocompleteSuggestion(u"Val1", MatchingType::kName),
  };
  std::vector<Suggestion> suggestions2 = {
      CreateAutocompleteSuggestion(u"Val1", MatchingType::kName),
      CreateAutocompleteSuggestion(u"Val2", MatchingType::kLabel),
  };

  logger().OnSuggestionsShown(field_id, suggestions1);
  logger().OnSuggestionsShown(field_id, suggestions2);
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance", false, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Name", false, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Label", false, 1);
}

// Verifies that per-field state is cleared upon form submission so that
// subsequent submissions do not re-emit metrics for already-submitted fields.
TEST_F(AutocompleteKeyMetricsLoggerTest, StateClearedAfterFormSubmission) {
  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  std::vector<Suggestion> suggestions = {
      CreateAutocompleteSuggestion(u"John", MatchingType::kName),
  };

  logger().OnSuggestionsShown(field_id, suggestions);
  logger().OnSuggestionSelected(field_id, suggestions[0]);
  test_api(form).field(0).set_value(u"John");

  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);
  // Submitting the same form again without new interactions should be a no-op.
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance.Name", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness.Name", true, 1);
}

// Verifies that when kAutofillLabelSensitiveAutocomplete is disabled, total
// metrics are still emitted, but no .{MatchingType} slices are recorded.
TEST_F(AutocompleteKeyMetricsLoggerTest, FeatureDisabled_OnlyTotalEmitted) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      features::kAutofillLabelSensitiveAutocomplete);

  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  Suggestion suggestion(u"Val", SuggestionType::kAutocompleteEntry);
  suggestion.payload = AutocompleteEntry(AutocompleteKey(u"name", u"Val"),
                                         base::Time::Now(), base::Time::Now());

  logger().OnSuggestionsShown(field_id, {suggestion});
  logger().OnSuggestionSelected(field_id, suggestion);

  test_api(form).field(0).set_value(u"Val");
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingAcceptance", true, 1);
  histogram_tester().ExpectUniqueSample(
      "Autocomplete.KeyMetrics.FillingCorrectness", true, 1);

  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingAcceptance.Name", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingAcceptance.Label", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingAcceptance.BothNameAndLabel", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.Name", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.Label", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness.BothNameAndLabel", 0);
}

// Verifies that suggestions that are not autocomplete entries do not produce
// any Autocomplete.KeyMetrics samples.
TEST_F(AutocompleteKeyMetricsLoggerTest, NonAutocompleteSuggestionsIgnored) {
  FormData form = test::CreateTestAddressFormData();
  FieldGlobalId field_id = form.fields()[0].global_id();

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kAddressEntry),
  };

  logger().OnSuggestionsShown(field_id, suggestions);
  logger().OnFormSubmitted(form, /*form_structure=*/nullptr);

  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingAcceptance", 0);
  histogram_tester().ExpectTotalCount(
      "Autocomplete.KeyMetrics.FillingCorrectness", 0);
}

// Verifies that when a parsed FormStructure is provided on form submission,
// Autocomplete.KeyMetrics.FillingAcceptance.ByFieldType is emitted for each
// predicted FieldType of fields where autocomplete suggestions were shown.
TEST_F(AutocompleteKeyMetricsLoggerTest, FillingAcceptanceByFieldType) {
  FormData form = test::GetFormData({
      .fields = {{.role = NAME_FIRST}, {.role = PASSPORT_NUMBER}},
  });
  FormStructure form_structure(form);
  test_api(form_structure).SetFieldTypes({NAME_FIRST, PASSPORT_NUMBER});

  std::vector<Suggestion> suggestions = {
      CreateAutocompleteSuggestion(u"Val", MatchingType::kName),
  };

  // Field 0 (NAME_FIRST): shown and accepted.
  logger().OnSuggestionsShown(form.fields()[0].global_id(), suggestions);
  logger().OnSuggestionSelected(form.fields()[0].global_id(), suggestions[0]);
  test_api(form).field(0).set_value(u"Val");

  // Field 1 (PASSPORT_NUMBER): shown and ignored.
  logger().OnSuggestionsShown(form.fields()[1].global_id(), suggestions);

  logger().OnFormSubmitted(form, &form_structure);

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Autocomplete.KeyMetrics.FillingAcceptance.ByFieldType"),
      BucketsAre(Bucket(GetBucketForAcceptanceMetricsGroupedByFieldType(
                            NAME_FIRST, /*suggestion_accepted=*/true),
                        1),
                 Bucket(GetBucketForAcceptanceMetricsGroupedByFieldType(
                            PASSPORT_NUMBER, /*suggestion_accepted=*/false),
                        1)));
}

}  // namespace
}  // namespace autofill::autofill_metrics
