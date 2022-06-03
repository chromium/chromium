// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/ukm_test_util.h"
#include <set>

namespace autofill_assistant {

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptShownToUsers(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kTriggerScriptShownToUserEntry,
                                 {kTriggerScriptShownToUser, kTriggerUiType});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptStarted(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kTriggerScriptStartedEntry,
                                 {kTriggerScriptStarted});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptFinished(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kTriggerScriptFinishedEntry,
                                 {kTriggerScriptFinished, kTriggerUiType});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptOnboarding(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kTriggerScriptOnboardingEntry,
                                 {kTriggerScriptOnboarding, kTriggerUiType});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInChromeTriggering(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kInChromeTriggeringEntry,
                                 {kInChromeTriggerAction});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerConditionEvaluationTime(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantTimingEntry,
                                 {kTriggerConditionTimingMs});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ToHumanReadableMetrics(
    const std::vector<std::pair<ukm::SourceId, std::vector<UkmEnumVariant>>>&
        input) {
  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> output;
  std::transform(
      input.begin(), input.end(), std::back_inserter(output),
      [&](const auto& impression) {
        ukm::TestUkmRecorder::HumanReadableUkmEntry transformed_impression;
        transformed_impression.source_id = impression.first;
        for (const auto& metric : impression.second) {
          transformed_impression.metrics.emplace(
              kUkmEnumMetricNames[metric.index()],
              absl::visit(GenericConvertToInt64(), metric));
        }
        return transformed_impression;
      });
  return output;
}

}  // namespace autofill_assistant

namespace ukm {

std::ostream& operator<<(
    std::ostream& out,
    const ukm::TestUkmRecorder::HumanReadableUkmEntry& input) {
  out << "source-id = " << input.source_id << ", metrics = [";
  std::string deliminator;
  for (const auto& metric : input.metrics) {
    out << deliminator << metric.first << " = " << metric.second;
    deliminator = ", ";
  }
  out << "]";
  return out;
}

}  // namespace ukm
