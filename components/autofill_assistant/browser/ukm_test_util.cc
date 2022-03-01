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

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmStartRequest(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(
      kStartRequestEntry, {kStarted, kCaller, kSource, kIntent, kExperiments});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmRegularScriptOnboarding(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kRegularScriptOnboardingEntry, {kOnboarding});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCompleteContactProfilesCount(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectContact,
                                 {kCompleteContactProfilesCount});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmIncompleteContactProfilesCount(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectContact,
                                 {kIncompleteContactProfilesCount});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialContactFieldsStatus(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectContact,
                                 {kInitialContactFieldsStatus});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmContactModified(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectContact,
                                 {kContactModified});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCompleteCreditCardsCount(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectCreditCard,
                                 {kCompleteCreditCardsCount});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmIncompleteCreditCardsCount(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectCreditCard,
                                 {kIncompleteCreditCardsCount});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialCreditCardFieldsStatus(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectCreditCard,
                                 {kInitialCreditCardFieldsStatus});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialBillingAddressFieldsStatus(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectCreditCard,
                                 {kInitialBillingAddressFieldsStatus});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCreditCardModified(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectCreditCard,
                                 {kCreditCardModified});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCompleteShippingProfilesCount(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectShippingAddress,
                                 {kCompleteShippingProfilesCount});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmIncompleteShippingProfilesCount(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectShippingAddress,
                                 {kIncompleteShippingProfilesCount});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialShippingFieldsStatus(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectShippingAddress,
                                 {kInitialShippingFieldsStatus});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmShippingModified(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectShippingAddress,
                                 {kShippingModified});
}

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCollectUserDataResult(ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectUserDataResult,
                                 {kResult});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmTimeTakenMs(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectUserDataResult,
                                 {kTimeTakenMs});
}
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmUserDataSource(
    ukm::TestAutoSetUkmRecorder& ukm_recorder) {
  return ukm_recorder.GetEntries(kAutofillAssistantCollectUserDataResult,
                                 {kUserDataSource});
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

ukm::TestUkmRecorder::HumanReadableUkmEntry ToHumanReadableEntry(
    const ukm::SourceId& id,
    const std::string& metric_identifier,
    int64_t entry) {
  ukm::TestUkmRecorder::HumanReadableUkmEntry impression;
  impression.source_id = id;
  impression.metrics.emplace(metric_identifier, entry);
  return impression;
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
