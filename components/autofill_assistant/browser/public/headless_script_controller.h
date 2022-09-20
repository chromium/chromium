// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_SCRIPT_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_SCRIPT_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/headless_onboarding_result.h"

namespace autofill_assistant {

// Allows to execute AutofillAssistant scripts.
class HeadlessScriptController {
 public:
  struct ScriptResult {
    // TODO(b/209429727): use canonical status codes instead.
    bool success = false;
    HeadlessOnboardingResult onboarding_result =
        HeadlessOnboardingResult::kUndefined;
  };

  // Fetches and executes the script specified by `script_parameters`.
  // At most one script can be executed at the same time, if a script is already
  // being executed at the time of this call it will return an error.
  // If this instance of `HeadlessScriptController` is destroyed, the script
  // execution will be interrupted.
  virtual void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback) = 0;

  // Fetches and executes the script as specified above. In addition, it
  // it contains parameters to control whether to show Autofill assistant's
  // onboarding before a script is run and whether to suppress browsing features
  // (e.g. the keyboard and Autofill) while running.
  virtual void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback,
      bool use_autofill_assistant_onboarding,
      base::OnceCallback<void()> onboarding_successful_callback,
      bool suppress_browsing_features) = 0;

  virtual ~HeadlessScriptController() = default;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_SCRIPT_CONTROLLER_H_
