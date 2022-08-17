// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_FIELD_TRIAL_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_FIELD_TRIAL_UTIL_H_

#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/script_parameters.h"

namespace autofill_assistant {

class AssistantFieldTrialUtil {
 public:
  // The number of different synthetic trials. The value of the script parameter
  // FIELD_TRIAL_N will be used as the group name in the synthetic field trial
  // AutofillAssistantExperimentsTrial-N, where N is between 1 and
  // kSyntheticTrialParamCount (both inclusive).
  static constexpr int kSyntheticTrialParamCount = 5;

  virtual ~AssistantFieldTrialUtil() = default;

  virtual bool RegisterSyntheticFieldTrial(
      base::StringPiece trial_name,
      base::StringPiece group_name) const = 0;

  void RegisterSyntheticFieldTrialsForParameters(
      const ScriptParameters& parameters);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_FIELD_TRIAL_UTIL_H_
