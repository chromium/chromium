// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom-forward.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace autofill {

class AutofillClient;
class CustomFormControlScriptIndex;

// Will run the custom form control script of the main frame's document, i.e.
// the script that detects and fills the document's custom form controls (e.g.
// select-like widgets that aren't <select> elements). For now, when forms are
// first seen in the main frame's document, it looks up the script for its URL
// in the `CustomFormControlScriptIndex`, loads it into the renderer through
// `mojom::CustomFormControlAgent`, and detects the document's custom form
// controls with it. Other frames (e.g. iframes) aren't supported.
//
// One instance per tab, owned by `ChromeAutofillClient`, which creates it if
// the browser has a `CustomFormControlScriptIndex` (i.e. if
// `features::kAutofillCustomFormControlScripts` is enabled and a script
// directory is set). The index is owned by `GlobalFeatures` and outlives all
// tabs.
//
// TODO(b/578383598): Fill the custom form controls.
class CustomFormControlScriptRunner : public AutofillManager::Observer {
 public:
  // A custom form control that the script of the main frame's current
  // document detected, and that can be filled now (e.g. it is visible and
  // enabled).
  struct DetectedCustomControl {
    // The control's element, identified by its renderer id.
    FieldRendererId field_id;
    // The type that the script assigned. Always a type with a group, so never
    // e.g. `UNKNOWN_TYPE`.
    FieldType field_type;
  };

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
  friend class CustomFormControlScriptRunnerTestApi;

  // Forgets `frame_token_` and the detected controls, and drops the pending
  // replies for them.
  void Reset();

  // Detects the custom form controls in `manager`'s frame if its script loaded.
  // `Reset()` cancels the detection, e.g. if a navigation replaces the document
  // `document_token`.
  void OnScriptLoaded(base::WeakPtr<AutofillManager> manager,
                      blink::DocumentToken document_token,
                      bool success);

  // Replaces the stored controls with the fillable `controls`, or with none and
  // reports a bad message if they violate the
  // `mojom::DetectedCustomFormControl` requirements.
  void OnCustomFormControlsDetected(
      std::vector<mojom::DetectedCustomFormControlPtr> controls);

  const raw_ref<const CustomFormControlScriptIndex> index_;

  // The primary main frame whose current document has been looked up in
  // `index_`. Reset when the frame gets a new document or stops being the
  // primary main frame.
  std::optional<LocalFrameToken> frame_token_;

  // The custom form controls that the script of `frame_token_`'s document
  // detected.
  std::vector<DetectedCustomControl> detected_controls_;

  base::RepeatingCallback<void(const std::string&)>
      script_found_callback_for_testing_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  // Invalidated by `Reset()`, so that replies for a previous document are
  // dropped.
  base::WeakPtrFactory<CustomFormControlScriptRunner> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_
