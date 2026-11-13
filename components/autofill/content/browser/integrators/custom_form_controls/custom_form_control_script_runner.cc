// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// Returns whether custom selects of `field_type` may be filled. The option to
// select is passed to the site's script, so the script learns the value. Hence
// only non-sensitive types that custom selects typically have are supported.
bool IsSupportedType(FieldType field_type) {
  static constexpr FieldTypeSet kSupportedTypes = {
      ADDRESS_HOME_COUNTRY, ADDRESS_HOME_STATE, ADDRESS_HOME_CITY,
      PHONE_HOME_COUNTRY_CODE};
  return kSupportedTypes.contains(field_type);
}

ContentAutofillDriver& GetDriver(AutofillManager& manager) {
  return static_cast<ContentAutofillDriver&>(manager.driver());
}

}  // namespace

CustomFormControlScriptRunner::CustomFormControlScriptRunner(
    AutofillClient& client,
    const CustomFormControlScriptIndex* index)
    : client_(client), index_(CHECK_DEREF(index)) {
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
  // prerendered pages or pages in the BFCache. Its script is loaded only once:
  // the first time the renderer reports its forms.
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
  driver.GetCustomFormControlAgent()->LoadUnsafeScript(document_token, *script,
                                                       base::DoNothing());
}

void CustomFormControlScriptRunner::Reset() {
  frame_token_.reset();
  detected_controls_.clear();
  filling_state_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void CustomFormControlScriptRunner::DetectControls(AutofillManager& manager,
                                                   base::OnceClosure callback) {
  ContentAutofillDriver& driver = GetDriver(manager);
  driver.GetCustomFormControlAgent()->DetectCustomFormControls(
      driver.render_frame_host()->GetDocumentToken(),
      base::BindOnce(
          &CustomFormControlScriptRunner::OnCustomFormControlsDetected,
          weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(callback)));
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
    // Controls of unknown or unsupported types (see `IsSupportedType()`) can't
    // be filled, and inactive controls (e.g. hidden or disabled ones) can't be
    // filled now.
    const FieldType field_type = TypeNameToFieldType(control->field_type);
    if (!IsSupportedType(field_type) || !control->is_active) {
      continue;
    }
    detected_controls.push_back(
        {.field_id = control->field_id, .field_type = field_type});
  }
  detected_controls_ = std::move(detected_controls);
}

void CustomFormControlScriptRunner::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId trigger_field_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
        skip_reasons,
    const FillingPayload& filling_payload) {
  // Only fills (not previews) of forms of `frame_token_`'s document with an
  // address profile fill the custom selects. The runner observes the managers
  // of all frames, so the `frame_token_` check ignores fills in other frames
  // (e.g. iframes), and in a new main frame whose script wasn't injected yet.
  const AutofillProfile* const* profile =
      std::get_if<const AutofillProfile*>(&filling_payload);
  if (action_persistence != mojom::ActionPersistence::kFill || !profile ||
      !*profile || frame_token_ != manager.driver().GetFrameToken() ||
      filling_state_) {
    return;
  }
  filling_state_.emplace(**profile);
  DetectControls(
      manager,
      base::BindOnce(&CustomFormControlScriptRunner::FillNextControl,
                     weak_ptr_factory_.GetWeakPtr(), manager.GetWeakPtr()));
}

void CustomFormControlScriptRunner::FillNextControl(
    base::WeakPtr<AutofillManager> manager) {
  // `filling_state_` is set: `Reset()` ends the fill and invalidates the
  // pending replies that would call this, and the fill only ends when none is
  // pending.
  if (!manager || filling_state_->next_control >= detected_controls_.size()) {
    filling_state_.reset();
    return;
  }
  // `detected_controls_` and `frame_token_`'s document don't change during the
  // fill: `Reset()` ends the fill.
  const DetectedCustomControl& control =
      detected_controls_.at(filling_state_->next_control);
  filling_state_->next_control++;
  ContentAutofillDriver& driver = GetDriver(*manager);
  driver.GetCustomFormControlAgent()->ExtractOptions(
      driver.render_frame_host()->GetDocumentToken(), control.field_id,
      base::BindOnce(&CustomFormControlScriptRunner::OnOptionsExtracted,
                     weak_ptr_factory_.GetWeakPtr(), manager, control));
}

void CustomFormControlScriptRunner::OnOptionsExtracted(
    base::WeakPtr<AutofillManager> manager,
    DetectedCustomControl control,
    mojom::ExtractedCustomFormControlOptionsPtr options) {
  if (!manager) {
    filling_state_.reset();
    return;
  }
  // If the options couldn't be extracted, skip the control. Without options,
  // the reply doesn't say whether a popover was left open, so none is closed.
  if (!options) {
    FillNextControl(manager);
    return;
  }
  // The renderer only sends valid options (see
  // `mojom::ExtractedCustomFormControlOptions`), so invalid ones (e.g. too
  // many or too long ones) indicate a compromised renderer.
  std::vector<SelectOption> select_options = base::ToVector(
      options->options, [](const mojom::SelectOptionPtr& option) {
        return SelectOption{.value = option->value, .text = option->text};
      });
  if (!IsValidOptionVector(select_options)) {
    filling_state_.reset();
    mojo::ReportBadMessage("Invalid custom form control options");
    return;
  }
  ApplyOption(*manager, control.field_id,
              ChooseOption(control.field_type, std::move(select_options)),
              options->popover_needs_to_be_closed,
              base::BindOnce(&CustomFormControlScriptRunner::FillNextControl,
                             weak_ptr_factory_.GetWeakPtr(), manager));
}

std::optional<SelectOption> CustomFormControlScriptRunner::ChooseOption(
    FieldType field_type,
    std::vector<SelectOption> options) const {
  // The matching is the same as for <select> elements, e.g. states match by
  // name or abbreviation.
  AutofillField field;
  field.set_form_control_type(FormControlType::kSelectOne);
  field.set_options(std::move(options));
  FillingValueAndType filling = GetFillingValueAndTypeForProfile(
      filling_state_->profile,
      std::string(client_->GetAppLocale().tag_string()), field_type, field,
      client_->GetAddressNormalizer());
  // For <select> elements, `select_text` is set iff an option matches. The
  // option's value may be empty.
  if (!filling.select_text) {
    return std::nullopt;
  }
  return SelectOption{.value = std::move(filling.value),
                      .text = std::move(*filling.select_text)};
}

void CustomFormControlScriptRunner::ApplyOption(
    AutofillManager& manager,
    FieldRendererId field,
    std::optional<SelectOption> option,
    bool popover_needs_to_be_closed,
    base::OnceClosure callback) {
  ContentAutofillDriver& driver = GetDriver(manager);
  const blink::DocumentToken document_token =
      driver.render_frame_host()->GetDocumentToken();
  if (option) {
    // Selecting the option also closes the popover.
    driver.GetCustomFormControlAgent()->SetValue(
        document_token, field,
        mojom::SelectOption::New(std::move(option->value),
                                 std::move(option->text)),
        std::move(callback));
  } else if (popover_needs_to_be_closed) {
    driver.GetCustomFormControlAgent()->ClosePopover(document_token, field,
                                                     std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

}  // namespace autofill
