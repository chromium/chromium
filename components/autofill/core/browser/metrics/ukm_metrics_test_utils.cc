// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

using ::testing::ElementsAreArray;
using ::testing::Matcher;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAreArray;
using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmInteractedWithFormType = ukm::builders::Autofill_InteractedWithForm;
using UkmSuggestionsShownType = ukm::builders::Autofill_SuggestionsShown;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmTextFieldValueChangedType = ukm::builders::Autofill_TextFieldDidChange;
using UkmLogHiddenRepresentationalFieldSkipDecisionType =
    ukm::builders::Autofill_HiddenRepresentationalFieldSkipDecision;
using UkmFieldTypeValidationType = ukm::builders::Autofill_FieldTypeValidation;
using UkmFieldFillStatusType = ukm::builders::Autofill_FieldFillStatus;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using UkmEditedAutofilledFieldAtSubmission =
    ukm::builders::Autofill_EditedAutofilledFieldAtSubmission;
using UkmAutofillKeyMetricsType = ukm::builders::Autofill_KeyMetrics;
using UkmFieldInfoType = ukm::builders::Autofill2_FieldInfo;

// Clears any time duration metrics metrics whose value is non-negative.
std::vector<UkmMetricNameAndValue> ResetMillisecondsSinceParse(
    std::vector<UkmMetricNameAndValue> metrics) {
  for (UkmMetricNameAndValue& metric : metrics) {
    if (metric.metric_name == "MillisecondsSinceFormParsed" &&
        metric.value > 0) {
      metric.value = 0;
    }
  }
  return metrics;
}

// Turns an event and metric hash into a human-readable name.
// The name is only the metric's name. It does not include the event's name.
std::string_view GetMetricName(uint64_t event_hash, uint64_t metric_hash) {
  static base::NoDestructor<ukm::builders::DecodeMap> decode_map(
      ukm::builders::CreateDecodeMap());
  auto outer_it = decode_map->find(event_hash);
  if (outer_it == decode_map->end()) {
    LOG(ERROR) << "Unknown event hash " << event_hash;
    return "<Unknown event hash>";
  }
  auto inner_it = outer_it->second.metric_map.find(metric_hash);
  if (inner_it == outer_it->second.metric_map.end()) {
    LOG(ERROR) << "Unknown metric hash " << metric_hash << " for metric "
               << outer_it->second.name;
    return "<Unknown metric hash>";
  }
  return inner_it->second;
}

std::vector<UkmMetricNameAndValue> UkmEntryToUkmMetricNameAndValues(
    const ukm::mojom::UkmEntry* ukm_entry) {
  return ResetMillisecondsSinceParse(base::ToVector(
      ukm_entry->metrics, [&](const std::pair<uint64_t, int64_t>& p) {
        const uint64_t metric_hash = p.first;
        const int64_t value = p.second;
        return UkmMetricNameAndValue(
            GetMetricName(ukm_entry->event_hash, metric_hash), value);
      }));
}

}  // namespace

FormSignature Collapse(FormSignature sig) {
  return FormSignature(sig.value() % 1021);
}

FieldSignature Collapse(FieldSignature sig) {
  return FieldSignature(sig.value() % 1021);
}

void PrintTo(const UkmMetricNameAndValue& metric, std::ostream* os) {
  *os << "{\"" << metric.metric_name << "\", " << metric.value << "}";
}

std::vector<std::vector<UkmMetricNameAndValue>> GetUkmEvents(
    const ukm::TestUkmRecorder& ukm_recorder,
    std::string_view event_name) {
  return base::ToVector(ukm_recorder.GetEntriesByName(event_name),
                        UkmEntryToUkmMetricNameAndValues);
}

Matcher<const std::vector<std::vector<UkmMetricNameAndValue>>&> UkmEventsAre(
    std::vector<std::vector<UkmMetricNameAndValue>> expected_events) {
  return ElementsAreArray(base::ToVector(
      expected_events,
      [](std::vector<UkmMetricNameAndValue>& expected_metrics) {
        return UnorderedElementsAreArray(
            ResetMillisecondsSinceParse(std::move(expected_metrics)));
      }));
}

std::vector<GURL> GetEventUrls(const ukm::TestUkmRecorder& ukm_recorder,
                               std::string_view event_name) {
  return base::ToVector(
      ukm_recorder.GetEntriesByName(event_name), [&](const auto& ukm_entry) {
        const ukm::UkmSource& ukm_source = CHECK_DEREF(
            ukm_recorder.GetSourceForSourceId(ukm_entry->source_id));
        return ukm_source.url();
      });
}

}  // namespace autofill::autofill_metrics
