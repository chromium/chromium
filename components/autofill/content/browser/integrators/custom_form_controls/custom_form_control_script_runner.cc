// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace autofill {

CustomFormControlScriptRunner::CustomFormControlScriptRunner(
    AutofillClient& client,
    const CustomFormControlScriptIndex* index)
    : index_(CHECK_DEREF(index)) {
  autofill_managers_observation_.Observe(&client);
}

CustomFormControlScriptRunner::~CustomFormControlScriptRunner() = default;

void CustomFormControlScriptRunner::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState previous,
    AutofillManager::LifecycleState new_state) {
  // If `frame_token_` has a new document (`kPendingReset`), is going away, or
  // isn't the primary main frame anymore (e.g. it was put into the BFCache),
  // then the next document needs its own script and detected controls.
  // TODO(b/578383598): Figure out how to handle pages in the BFCache and
  // prerendered pages, whose forms may be seen before they become the primary
  // main frame.
  if (new_state != AutofillManager::LifecycleState::kActive &&
      frame_token_ == manager.driver().GetFrameToken()) {
    Reset();
  }
}

void CustomFormControlScriptRunner::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  if (updated_forms.empty() || !index_->is_initialized()) {
    return;
  }
  ContentAutofillDriver& driver =
      static_cast<ContentAutofillDriver&>(manager.driver());
  content::RenderFrameHost* rfh = driver.render_frame_host();
  // Only the primary main frame's document gets a script, not e.g. iframes,
  // prerendered pages or pages in the BFCache. Its script is loaded, and its
  // custom form controls are detected, only once: the first time the renderer
  // reports its forms.
  if (!rfh->IsInPrimaryMainFrame() || frame_token_ == driver.GetFrameToken()) {
    return;
  }
  // A new primary main frame (e.g. after a cross-document navigation) may
  // report forms before the previous one is reset (e.g. before it is unloaded),
  // so forget the previous frame's script and controls.
  Reset();
  frame_token_ = driver.GetFrameToken();
  const blink::DocumentToken document_token = rfh->GetDocumentToken();
  std::optional<std::string> script =
      index_->FindScript(rfh->GetLastCommittedURL());
  if (!script) {
    return;
  }
  if (script_found_callback_for_testing_) {
    script_found_callback_for_testing_.Run(*script);
  }
  driver.GetCustomFormControlAgent()->LoadUnsafeScript(
      document_token, *script,
      base::BindOnce(&CustomFormControlScriptRunner::OnScriptLoaded,
                     weak_ptr_factory_.GetWeakPtr(), manager.GetWeakPtr(),
                     document_token));
}

void CustomFormControlScriptRunner::Reset() {
  frame_token_.reset();
  detected_controls_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void CustomFormControlScriptRunner::OnScriptLoaded(
    base::WeakPtr<AutofillManager> manager,
    blink::DocumentToken document_token,
    bool success) {
  if (!success || !manager) {
    return;
  }
  static_cast<ContentAutofillDriver&>(manager->driver())
      .GetCustomFormControlAgent()
      ->DetectCustomFormControls(
          document_token,
          base::BindOnce(
              &CustomFormControlScriptRunner::OnCustomFormControlsDetected,
              weak_ptr_factory_.GetWeakPtr()));
}

void CustomFormControlScriptRunner::OnCustomFormControlsDetected(
    std::vector<mojom::DetectedCustomFormControlPtr> controls) {
  // The renderer only sends at most `kMaxExtractableFields` controls, with
  // different non-null `FieldRendererId`s (see
  // `mojom::DetectedCustomFormControl`), so a violation indicates a compromised
  // renderer.
  detected_controls_.clear();
  if (controls.size() > kMaxExtractableFields) {
    mojo::ReportBadMessage("Too many custom form controls");
    return;
  }
  std::vector<DetectedCustomControl> detected_controls;
  base::flat_set<FieldRendererId> field_ids;
  for (const mojom::DetectedCustomFormControlPtr& control : controls) {
    if (!control->field_id) {
      mojo::ReportBadMessage("Custom form control without renderer id");
      return;
    }
    if (!field_ids.insert(control->field_id).second) {
      mojo::ReportBadMessage("Duplicate custom form control renderer id");
      return;
    }
    // Controls of unknown types or of meta types without a group (e.g.
    // `NO_SERVER_DATA`) can't be filled, and inactive controls (e.g. hidden or
    // disabled ones) can't be filled now.
    const FieldType field_type = TypeNameToFieldType(control->field_type);
    if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kNoGroup ||
        !control->is_active) {
      continue;
    }
    detected_controls.push_back(
        {.field_id = control->field_id, .field_type = field_type});
  }
  detected_controls_ = std::move(detected_controls);
}

}  // namespace autofill
