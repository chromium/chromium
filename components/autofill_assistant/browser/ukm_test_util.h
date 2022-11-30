// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UKM_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UKM_TEST_UTIL_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill_assistant {

// The identifiers for all UKM entries that we currently record/test.
const char kTriggerScriptShownToUserEntry[] =
    "AutofillAssistant.LiteScriptShownToUser";
const char kTriggerScriptStartedEntry[] = "AutofillAssistant.LiteScriptStarted";
const char kTriggerScriptFinishedEntry[] =
    "AutofillAssistant.LiteScriptFinished";
const char kTriggerScriptOnboardingEntry[] =
    "AutofillAssistant.LiteScriptOnboarding";
const char kInChromeTriggeringEntry[] = "AutofillAssistant.InChromeTriggering";
const char kAutofillAssistantTimingEntry[] = "AutofillAssistant.Timing";
const char kStartRequestEntry[] = "AutofillAssistant.StartRequest";
const char kRegularScriptOnboardingEntry[] =
    "AutofillAssistant.RegularScriptOnboarding";
const char kAutofillAssistantCollectContact[] =
    "AutofillAssistant.CollectContact";
const char kAutofillAssistantCollectCreditCard[] =
    "AutofillAssistant.CollectPayment";
const char kAutofillAssistantCollectShippingAddress[] =
    "AutofillAssistant.CollectShippingAddress";
const char kAutofillAssistantCollectUserDataResult[] =
    "AutofillAssistant.CollectUserDataResult";
const char kAutofillAssistantFlowFinishedEntry[] =
    "AutofillAssistant.FlowFinished";

// The identifiers for all UKM metrics that we currently record/test.
const char kTriggerUiType[] = "TriggerUIType";
const char kTriggerScriptShownToUser[] = "LiteScriptShownToUser";
const char kTriggerScriptStarted[] = "LiteScriptStarted";
const char kTriggerScriptFinished[] = "LiteScriptFinished";
const char kTriggerScriptOnboarding[] = "LiteScriptOnboarding";
const char kInChromeTriggerAction[] = "InChromeTriggerAction";
const char kTriggerConditionTimingMs[] = "TriggerConditionEvaluationMs";
const char kStarted[] = "Started";
const char kCaller[] = "Caller";
const char kSource[] = "Source";
const char kIntent[] = "Intent";
const char kExperiments[] = "Experiments";
const char kOnboarding[] = "Onboarding";
const char kCompleteContactProfilesCount[] = "CompleteContactProfilesCount";
const char kIncompleteContactProfilesCount[] = "IncompleteContactProfilesCount";
const char kInitialContactFieldsStatus[] = "InitialContactFieldsStatus";
const char kContactModified[] = "ContactModified";
const char kCompleteCreditCardsCount[] = "CompleteCreditCardsCount";
const char kIncompleteCreditCardsCount[] = "IncompleteCreditCardsCount";
const char kInitialCreditCardFieldsStatus[] = "InitialCreditCardFieldsStatus";
const char kInitialBillingAddressFieldsStatus[] =
    "InitialBillingAddressFieldsStatus";
const char kCreditCardModified[] = "CreditCardModified";
const char kCompleteShippingProfilesCount[] = "CompleteShippingProfilesCount";
const char kIncompleteShippingProfilesCount[] =
    "IncompleteShippingProfilesCount";
const char kInitialShippingFieldsStatus[] = "InitialShippingFieldsStatus";
const char kShippingModified[] = "ShippingModified";
const char kResult[] = "Result";
const char kTimeTakenMs[] = "TimeTakenMs";
const char kUserDataSource[] = "UserDataSource";
const char kFlowFinishedState[] = "FlowFinishedState";
const char kFlowFinishedNumActions[] = "NumActions";
const char kFlowFinishedNumJsFlowActions[] = "NumJsFlowActions";
const char kFlowFinishedNumRoundtrips[] = "NumRoundtrips";
const char kFlowFinishedTotalDecodedGetActionsSizeInBytes[] =
    "TotalDecodedGetActionsSizeInBytes";
const char kFlowFinishedTotalDecodedJsFlowSizeInBytes[] =
    "TotalDecodedJsFlowSizeInBytes";
const char kFlowFinishedTotalEncodedGetActionsSizeInBytes[] =
    "TotalEncodedGetActionsSizeInBytes";

// Convenience accessors for UKM metrics.
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptShownToUsers(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptStarted(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptFinished(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerScriptOnboarding(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInChromeTriggering(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmTriggerConditionEvaluationTime(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmStartRequest(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmRegularScriptOnboarding(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCompleteContactProfilesCount(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmIncompleteContactProfilesCount(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialContactFieldsStatus(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmContactModified(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCompleteCreditCardsCount(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmIncompleteCreditCardsCount(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialCreditCardFieldsStatus(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialBillingAddressFieldsStatus(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCreditCardModified(ukm::TestAutoSetUkmRecorder& ukm_recorder);

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCompleteShippingProfilesCount(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmIncompleteShippingProfilesCount(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmInitialShippingFieldsStatus(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmShippingModified(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry>
GetUkmCollectUserDataResult(ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmTimeTakenMs(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmUserDataSource(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);

std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> GetUkmFlowFinished(
    ukm::TestAutoSetUkmRecorder& ukm_recorder);

// Variant containing all UKM enums that we currently record/test.
// NOTE: When adding entries, remember to also modify kUkmEnumMetricNames!
using UkmEnumVariant = absl::variant<TriggerScriptProto::TriggerUIType,
                                     Metrics::TriggerScriptShownToUser,
                                     Metrics::TriggerScriptStarted,
                                     Metrics::TriggerScriptFinishedState,
                                     Metrics::TriggerScriptOnboarding,
                                     Metrics::InChromeTriggerAction,
                                     Metrics::AutofillAssistantStarted,
                                     Metrics::AutofillAssistantCaller,
                                     Metrics::AutofillAssistantSource,
                                     AutofillAssistantIntent,
                                     Metrics::AutofillAssistantExperiment,
                                     Metrics::Onboarding,
                                     Metrics::FlowFinishedState>;

// The metric names corresponding to the variant alternatives of UkmEnumVariant.
// NOTE: When adding entries, remember to also modify UkmEnumVariant!
const std::vector<std::string> kUkmEnumMetricNames = {kTriggerUiType,
                                                      kTriggerScriptShownToUser,
                                                      kTriggerScriptStarted,
                                                      kTriggerScriptFinished,
                                                      kTriggerScriptOnboarding,
                                                      kInChromeTriggerAction,
                                                      kStarted,
                                                      kCaller,
                                                      kSource,
                                                      kIntent,
                                                      kExperiments,
                                                      kOnboarding,
                                                      kFlowFinishedState};

// Intended to be used to convert a UkmEnumVariant to int64_t using a visitor.
// Usage:
// UkmEnumVariant v = Metrics::TriggerScriptShownToUser::SHOWN_TO_USER;
// int64_t i = absl::visit(GenericConvertToInt64(), v);
struct GenericConvertToInt64 {
  template <typename T>
  int64_t operator()(const T& input) {
    return static_cast<int64_t>(input);
  }
};

// Converts |metrics| into a vector of ukm readable metrics that can be used
// directly in test expectations.
// Usage:
// EXPECT_THAT(
//     ukm_recorder.GetEntries("SomeEntry", {"MetricA", "MetricB"}),
//     ElementsAreArray(ToHumanReadableMetrics(
//       {navigation_ids[0], {MetricA::X, MetricB::Q}},
//       {navigation_ids[1], {MetricA::Y, MetricB::P}})));
std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ToHumanReadableMetrics(
    const std::vector<std::pair<ukm::SourceId, std::vector<UkmEnumVariant>>>&
        metrics);

ukm::TestUkmRecorder::HumanReadableUkmEntry ToHumanReadableEntry(
    const ukm::SourceId& id,
    const std::string& metric_identifier,
    int64_t entry);

}  // namespace autofill_assistant

namespace ukm {
// Despite its name, a HumanReadableUkmEntry is not actually readable unless we
// define a custom output operator for it.
std::ostream& operator<<(
    std::ostream& out,
    const ukm::TestUkmRecorder::HumanReadableUkmEntry& input);
}  // namespace ukm

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UKM_TEST_UTIL_H_
