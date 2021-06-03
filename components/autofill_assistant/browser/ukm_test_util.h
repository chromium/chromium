// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UKM_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UKM_TEST_UTIL_H_

#include <map>
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

// The identifiers for all UKM metrics that we currently record/test.
const char kTriggerUiType[] = "TriggerUIType";
const char kTriggerScriptShownToUser[] = "LiteScriptShownToUser";
const char kTriggerScriptStarted[] = "LiteScriptStarted";
const char kTriggerScriptFinished[] = "LiteScriptFinished";
const char kTriggerScriptOnboarding[] = "LiteScriptOnboarding";
const char kInChromeTriggerAction[] = "InChromeTriggerAction";

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

// Variant containing all UKM enums that we currently record/test.
// NOTE: When adding entries, remember to also modify kUkmEnumMetricNames!
using UkmEnumVariant = absl::variant<TriggerScriptProto::TriggerUIType,
                                     Metrics::TriggerScriptShownToUser,
                                     Metrics::TriggerScriptStarted,
                                     Metrics::TriggerScriptFinishedState,
                                     Metrics::TriggerScriptOnboarding,
                                     Metrics::InChromeTriggerAction>;

// The metric names corresponding to the variant alternatives of UkmEnumVariant.
// NOTE: When adding entries, remember to also modify UkmEnumVariant!
const std::vector<std::string> kUkmEnumMetricNames = {
    kTriggerUiType,         kTriggerScriptShownToUser, kTriggerScriptStarted,
    kTriggerScriptFinished, kTriggerScriptOnboarding,  kInChromeTriggerAction};

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

}  // namespace autofill_assistant

namespace ukm {
// Despite its name, a HumanReadableUkmEntry is not actually readable unless we
// define a custom output operator for it.
std::ostream& operator<<(
    std::ostream& out,
    const ukm::TestUkmRecorder::HumanReadableUkmEntry& input);
}  // namespace ukm

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UKM_TEST_UTIL_H_
