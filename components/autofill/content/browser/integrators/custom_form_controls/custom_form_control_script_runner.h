// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_

#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;
class CustomFormControlScriptIndex;

// Will run the custom form control script of each document, i.e. the script
// that detects and fills the document's custom form controls (e.g. select-like
// widgets that aren't <select> elements). For now, when forms are first seen in
// a document, it only looks up the script for the frame's URL in the
// `CustomFormControlScriptIndex`.
//
// One instance per tab, owned by `ChromeAutofillClient`, which creates it if
// the browser has a `CustomFormControlScriptIndex` (i.e. if
// `features::kAutofillCustomFormControlScripts` is enabled and a script
// directory is set). The index is owned by `GlobalFeatures` and outlives all
// tabs.
//
// TODO(b/578383598): Load the script into the frame's renderer, detect the
// custom form controls and fill them.
class CustomFormControlScriptRunner : public AutofillManager::Observer {
 public:
  // `index` must not be null and must outlive the runner.
  CustomFormControlScriptRunner(AutofillClient& client,
                                const CustomFormControlScriptIndex* index);
  CustomFormControlScriptRunner(const CustomFormControlScriptRunner&) = delete;
  CustomFormControlScriptRunner& operator=(
      const CustomFormControlScriptRunner&) = delete;
  ~CustomFormControlScriptRunner() override;

  // Runs `callback` with the script whenever one is found for a document.
  void set_script_found_callback_for_testing(
      base::RepeatingCallback<void(const std::string&)> callback) {
    script_found_callback_for_testing_ = std::move(callback);
  }

  // AutofillManager::Observer:
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillManager::LifecycleState previous,
      AutofillManager::LifecycleState new_state) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;

 private:
  const raw_ref<const CustomFormControlScriptIndex> index_;

  // The frames whose current document has been looked up in `index_`.
  base::flat_set<LocalFrameToken> frames_looked_up_;

  base::RepeatingCallback<void(const std::string&)>
      script_found_callback_for_testing_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_
