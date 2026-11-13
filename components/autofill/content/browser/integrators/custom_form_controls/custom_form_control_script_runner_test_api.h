// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_TEST_API_H_

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner.h"

namespace autofill {

// Exposes some testing operations for `CustomFormControlScriptRunner`.
class CustomFormControlScriptRunnerTestApi {
 public:
  explicit CustomFormControlScriptRunnerTestApi(
      CustomFormControlScriptRunner* runner)
      : runner_(*runner) {}

  // Returns the custom form controls detected in the main frame's current
  // document.
  base::span<const CustomFormControlScriptRunner::DetectedCustomControl>
  detected_controls() const {
    return runner_->detected_controls_;
  }

  // Returns whether a fill of the detected controls is in progress.
  bool is_filling() const { return runner_->filling_state_.has_value(); }

 private:
  const raw_ref<CustomFormControlScriptRunner> runner_;
};

inline CustomFormControlScriptRunnerTestApi test_api(
    CustomFormControlScriptRunner& runner) {
  return CustomFormControlScriptRunnerTestApi(&runner);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_TEST_API_H_
