// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_field_trial_util.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill_assistant/browser/script_parameters.h"

namespace autofill_assistant {
namespace {
// Synthetic field trial names and group names should match those specified
// in google3/analysis/uma/dashboards/variations/
// .../generate_server_hashes.py and
// .../synthetic_trials.py
const char kTriggeredSyntheticTrial[] = "AutofillAssistantTriggered";
const char kEnabledGroupName[] = "Enabled";
const char kExperimentsSyntheticTrial[] = "AutofillAssistantExperimentsTrial";
}  // namespace

void AssistantFieldTrialUtil::RegisterSyntheticFieldTrialsForParameters(
    const ScriptParameters& parameters) {
  RegisterSyntheticFieldTrial(kTriggeredSyntheticTrial, kEnabledGroupName);
  // Synthetic trial for experiments.
  for (int i = 1; i <= kSyntheticTrialParamCount; ++i) {
    const absl::optional<std::string> group = parameters.GetFieldTrialGroup(i);
    if (group.has_value()) {
      const std::string trial_name = base::StrCat(
          {kExperimentsSyntheticTrial, "-", base::NumberToString(i)});
      RegisterSyntheticFieldTrial(trial_name, group.value());
    }
  }
  // Backwards compatibility.
  // TODO(b/242171397): Remove.
  const auto& experiments = parameters.GetExperiments();
  if (!experiments.empty()) {
    RegisterSyntheticFieldTrial(kExperimentsSyntheticTrial, experiments.back());
  }
}

}  // namespace autofill_assistant
