// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom-forward.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillClient;
class CustomFormControlScriptIndex;

// Will run the custom form control script of the main frame's document, i.e.
// the script that detects and fills the document's custom form controls (e.g.
// select-like widgets that aren't <select> elements). For now, when forms are
// first seen in the main frame's document, it looks up the script for its URL
// in the `CustomFormControlScriptIndex` and loads it into the renderer through
// `mojom::CustomFormControlAgent`. When a form of the document is then filled
// with an address profile, it detects the document's custom form controls with
// the script and fills the detected custom selects with the profile too.
// Other frames (e.g. iframes) aren't supported.
//
// One instance per tab, owned by `ChromeAutofillClient`, which creates it if
// the browser has a `CustomFormControlScriptIndex` (i.e. if
// `features::kAutofillCustomFormControlScripts` is enabled and a script
// directory is set). The index is owned by `GlobalFeatures` and outlives all
// tabs.
class CustomFormControlScriptRunner : public AutofillManager::Observer {
 public:
  // A custom form control that the script of the main frame's current
  // document detected, and that can be filled now (e.g. it is visible and
  // enabled).
  struct DetectedCustomControl {
    // The control's element, identified by its renderer id.
    FieldRendererId field_id;
    // The type that the script assigned.
    FieldType field_type;
  };

  // `client` and `index` must outlive the runner. `index` must not be null.
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
  void OnFillOrPreviewForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId trigger_field_id,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
          skip_reasons,
      const FillingPayload& filling_payload) override;

 private:
  friend class CustomFormControlScriptRunnerTestApi;

  // The state of a fill of `detected_controls_`.
  struct FillingState {
    // The profile to fill the controls with.
    AutofillProfile profile;
    // The index of the next control in `detected_controls_` to fill.
    size_t next_control = 0;
  };

  // Forgets `frame_token_` and the detected controls, ends the fill in
  // progress, and drops the pending replies for them.
  void Reset();

  // Detects and stores the custom form controls in `manager`'s frame (see
  // `OnCustomFormControlsDetected()`), then runs `callback`.
  void DetectControls(AutofillManager& manager, base::OnceClosure callback);

  // Replaces the stored controls with the fillable `controls`, or with none and
  // reports a bad message if they violate the
  // `mojom::DetectedCustomFormControl` requirements.
  void OnCustomFormControlsDetected(
      std::vector<mojom::DetectedCustomFormControlPtr> controls);

  // Extracts the options of the next control in `detected_controls_` to fill,
  // or ends the fill if there is none or `manager` is gone.
  void FillNextControl(base::WeakPtr<AutofillManager> manager);

  // Chooses the option among the extracted `options` of `control` to fill it
  // with and applies it (see `ApplyOption()`), or skips `control` if its
  // options couldn't be extracted. Then fills the next control. Reports a bad
  // message and ends the fill if `options` are invalid.
  void OnOptionsExtracted(base::WeakPtr<AutofillManager> manager,
                          DetectedCustomControl control,
                          mojom::ExtractedCustomFormControlOptionsPtr options);

  // Returns the option among `options` that matches the profile of the fill
  // for a control of `field_type`, or nullopt if none matches.
  std::optional<SelectOption> ChooseOption(
      FieldType field_type,
      std::vector<SelectOption> options) const;

  // Selects `option` in the control `field` of `manager`'s frame if there is
  // one, or else closes the control's popover if the extraction left it open.
  // Then runs `callback`.
  void ApplyOption(AutofillManager& manager,
                   FieldRendererId field,
                   std::optional<SelectOption> option,
                   bool popover_needs_to_be_closed,
                   base::OnceClosure callback);

  const raw_ref<AutofillClient> client_;
  const raw_ref<const CustomFormControlScriptIndex> index_;

  // The primary main frame whose current document has been looked up in
  // `index_`. Reset when the frame gets a new document or stops being the
  // primary main frame.
  std::optional<LocalFrameToken> frame_token_;

  // The custom form controls that the script of `frame_token_`'s document
  // detected when the last fill started.
  std::vector<DetectedCustomControl> detected_controls_;

  // Set while `detected_controls_` are being filled. The controls are filled
  // one after another, since filling one may open its popover. Fills that
  // start while one is in progress are ignored.
  std::optional<FillingState> filling_state_;

  base::RepeatingCallback<void(const std::string&)>
      script_found_callback_for_testing_;

  ScopedAutofillManagersObservation autofill_managers_observation_{this};

  // Invalidated by `Reset()`, so that replies for a previous document are
  // dropped.
  base::WeakPtrFactory<CustomFormControlScriptRunner> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_INTEGRATORS_CUSTOM_FORM_CONTROLS_CUSTOM_FORM_CONTROL_SCRIPT_RUNNER_H_
