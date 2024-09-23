// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmDeveloperEngagementType = ukm::builders::Autofill_DeveloperEngagement;
using UkmInteractedWithFormType = ukm::builders::Autofill_InteractedWithForm;
using UkmSuggestionsShownType = ukm::builders::Autofill_SuggestionsShown;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmTextFieldDidChangeType = ukm::builders::Autofill_TextFieldDidChange;
using UkmLogHiddenRepresentationalFieldSkipDecisionType =
    ukm::builders::Autofill_HiddenRepresentationalFieldSkipDecision;
using UkmLogRepeatedServerTypePredictionRationalized =
    ukm::builders::Autofill_RepeatedServerTypePredictionRationalized;
using UkmFieldTypeValidationType = ukm::builders::Autofill_FieldTypeValidation;
using UkmFieldFillStatusType = ukm::builders::Autofill_FieldFillStatus;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using UkmEditedAutofilledFieldAtSubmission =
    ukm::builders::Autofill_EditedAutofilledFieldAtSubmission;
using UkmAutofillKeyMetricsType = ukm::builders::Autofill_KeyMetrics;
using UkmFieldInfoType = ukm::builders::Autofill2_FieldInfo;

FormSignature Collapse(FormSignature sig) {
  return FormSignature(sig.value() % 1021);
}

FieldSignature Collapse(FieldSignature sig) {
  return FieldSignature(sig.value() % 1021);
}

MATCHER(CompareMetricsIgnoringMillisecondsSinceFormParsed, "") {
  const auto& lhs = ::testing::get<0>(arg);
  const std::pair<std::string, int64_t>& rhs = ::testing::get<1>(arg);
  return lhs.first == base::HashMetricName(rhs.first) &&
         (lhs.second == rhs.second ||
          (lhs.second > 0 &&
           rhs.first ==
               UkmSuggestionFilledType::kMillisecondsSinceFormParsedName));
}

}  // namespace

void VerifyUkm(
    const ukm::TestUkmRecorder* ukm_recorder,
    const FormData& form,
    const char* event_name,
    const std::vector<std::vector<ExpectedUkmMetricsPair>>& expected_metrics) {
  auto entries = ukm_recorder->GetEntriesByName(event_name);

  EXPECT_LE(entries.size(), expected_metrics.size());
  for (size_t i = 0; i < expected_metrics.size() && i < entries.size(); i++) {
    ukm_recorder->ExpectEntrySourceHasUrl(entries[i],
                                          form.main_frame_origin().GetURL());
    EXPECT_THAT(entries[i]->metrics,
                testing::UnorderedPointwise(
                    CompareMetricsIgnoringMillisecondsSinceFormParsed(),
                    expected_metrics[i]));
  }
}

void VerifyDeveloperEngagementUkm(
    const ukm::TestUkmRecorder* ukm_recorder,
    const FormData& form,
    const bool is_for_credit_card,
    const DenseSet<FormTypeNameForLogging>& form_types,
    const std::vector<int64_t>& expected_metric_values) {
  int expected_metric_value = 0;
  for (const auto it : expected_metric_values) {
    expected_metric_value |= 1 << it;
  }

  auto entries =
      ukm_recorder->GetEntriesByName(UkmDeveloperEngagementType::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    ukm_recorder->ExpectEntrySourceHasUrl(
        entry, GURL(form.main_frame_origin().GetURL()));
    EXPECT_EQ(4u, entry->metrics.size());
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kDeveloperEngagementName,
        expected_metric_value);
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kIsForCreditCardName,
        is_for_credit_card);
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormTypesName,
        AutofillMetrics::FormTypesToBitVector(form_types));
    ukm_recorder->ExpectEntryMetric(
        entry, UkmDeveloperEngagementType::kFormSignatureName,
        Collapse(CalculateFormSignature(form)).value());
  }
}

void AppendFieldFillStatusUkm(
    const FormData& form,
    std::vector<std::vector<ExpectedUkmMetricsPair>>* expected_metrics) {
  FormSignature form_signature = Collapse(CalculateFormSignature(form));
  int64_t metric_type = static_cast<int64_t>(AutofillMetrics::TYPE_SUBMISSION);
  for (const FormFieldData& field : form.fields()) {
    FieldSignature field_signature =
        Collapse(CalculateFieldSignatureForField(field));
    expected_metrics->push_back(
        {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFieldFillStatusType::kFormSignatureName, form_signature.value()},
         {UkmFieldFillStatusType::kFieldSignatureName, field_signature.value()},
         {UkmFieldFillStatusType::kValidationEventName, metric_type},
         {UkmTextFieldDidChangeType::kIsAutofilledName,
          field.is_autofilled() ? 1 : 0},
         {UkmFieldFillStatusType::kWasPreviouslyAutofilledName, 0}});
  }
}

void AppendFieldTypeUkm(
    const FormData& form,
    const std::vector<FieldType>& heuristic_types,
    const std::vector<FieldType>& server_types,
    const std::vector<FieldType>& actual_types,
    std::vector<std::vector<ExpectedUkmMetricsPair>>* expected_metrics) {
  ASSERT_EQ(heuristic_types.size(), form.fields().size());
  ASSERT_EQ(server_types.size(), form.fields().size());
  ASSERT_EQ(actual_types.size(), form.fields().size());
  FormSignature form_signature = Collapse(CalculateFormSignature(form));
  int64_t metric_type = static_cast<int64_t>(AutofillMetrics::TYPE_SUBMISSION);
  std::vector<int64_t> prediction_sources{
      AutofillMetrics::PREDICTION_SOURCE_HEURISTIC,
      AutofillMetrics::PREDICTION_SOURCE_SERVER,
      AutofillMetrics::PREDICTION_SOURCE_OVERALL};
  for (size_t i = 0; i < form.fields().size(); ++i) {
    const FormFieldData& field = form.fields()[i];
    FieldSignature field_signature =
        Collapse(CalculateFieldSignatureForField(field));
    for (int64_t source : prediction_sources) {
      int64_t predicted_type = static_cast<int64_t>(
          (source == AutofillMetrics::PREDICTION_SOURCE_SERVER
               ? server_types
               : heuristic_types)[i]);
      int64_t actual_type = static_cast<int64_t>(actual_types[i]);
      expected_metrics->push_back(
          {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
           {UkmFieldFillStatusType::kFormSignatureName, form_signature.value()},
           {UkmFieldFillStatusType::kFieldSignatureName,
            field_signature.value()},
           {UkmFieldFillStatusType::kValidationEventName, metric_type},
           {UkmFieldTypeValidationType::kPredictionSourceName, source},
           {UkmFieldTypeValidationType::kPredictedTypeName, predicted_type},
           {UkmFieldTypeValidationType::kActualTypeName, actual_type}});
    }
  }
}

}  // namespace autofill::autofill_metrics
