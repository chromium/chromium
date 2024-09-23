// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_driver_ios.h"

#import "base/check_deref.h"
#import "base/containers/contains.h"
#import "base/containers/to_vector.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/observer_list.h"
#import "components/autofill/core/browser/autofill_driver_router.h"
#import "components/autofill/core/browser/form_filler.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/accessibility/ax_tree_id.h"
#import "ui/gfx/geometry/rect_f.h"
#import "url/origin.h"

namespace autofill {

namespace {
// AutofillDriverIOS::router_ only ever routes between instances of
// AutofillDriverIOS, so this cast is safe.
AutofillDriverIOS* cast(AutofillDriver* driver) {
  return static_cast<AutofillDriverIOS*>(driver);
}

bool IsAcrossIframesEnabled() {
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillAcrossIframesIos);
}
}  // namespace

// static
AutofillDriverIOS* AutofillDriverIOS::FromWebStateAndWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  return AutofillDriverIOSFactory::FromWebState(web_state)->DriverForFrame(
      web_frame);
}

// static
AutofillDriverIOS* AutofillDriverIOS::FromWebStateAndLocalFrameToken(
    web::WebState* web_state,
    LocalFrameToken token) {
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state);
  web::WebFrame* frame = frames_manager->GetFrameWithId(token.ToString());
  return frame ? FromWebStateAndWebFrame(web_state, frame) : nullptr;
}

AutofillDriverIOS::AutofillDriverIOS(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    AutofillDriverRouter* router,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    base::PassKey<AutofillDriverIOSFactory> pass_key)
    : web_state_(web_state),
      web_frame_id_(web_frame ? web_frame->GetFrameId() : ""),
      bridge_(bridge),
      client_(*client),
      manager_(std::make_unique<BrowserAutofillManager>(this, app_locale)),
      router_(router) {
  manager_observation_.Observe(manager_.get());

  if (IsAcrossIframesEnabled()) {
    std::optional<base::UnguessableToken> token_temp =
        DeserializeJavaScriptFrameId(web_frame_id_);
    if (token_temp) {
      local_frame_token_ = LocalFrameToken(*token_temp);
    }
  }
}

AutofillDriverIOS::~AutofillDriverIOS() {
  Unregister();
}

LocalFrameToken AutofillDriverIOS::GetFrameToken() const {
  return local_frame_token_;
}

std::optional<LocalFrameToken> AutofillDriverIOS::Resolve(FrameToken query) {
  if (!IsAcrossIframesEnabled()) {
    return std::nullopt;
  }

  if (absl::holds_alternative<LocalFrameToken>(query)) {
    return absl::get<LocalFrameToken>(query);
  }
  CHECK(absl::holds_alternative<RemoteFrameToken>(query));
  auto remote_token = absl::get<RemoteFrameToken>(query);
  auto* registrar = ChildFrameRegistrar::FromWebState(web_state_);
  return registrar ? registrar->LookupChildFrame(remote_token) : std::nullopt;
}

AutofillDriverIOS* AutofillDriverIOS::GetParent() {
  return parent_.get();
}

AutofillClient& AutofillDriverIOS::GetAutofillClient() {
  return *client_;
}

BrowserAutofillManager& AutofillDriverIOS::GetAutofillManager() {
  return *manager_;
}

// Return true as iOS has no MPArch.
bool AutofillDriverIOS::IsActive() const {
  return true;
}

bool AutofillDriverIOS::IsInAnyMainFrame() const {
  web::WebFrame* frame = web_frame();
  return frame ? frame->IsMainFrame() : true;
}

bool AutofillDriverIOS::HasSharedAutofillPermission() const {
  // Give the shared-autofill permission to the main frame of the webstate by
  // default.
  if (IsInAnyMainFrame()) {
    return true;
  }

  // Also propagate that permission to the direct children of the main
  // frame on the same origin as the main frame.
  if (parent_ && parent_->web_frame() && parent_->IsInAnyMainFrame() &&
      web_frame()) {
    return parent_->web_frame()->GetSecurityOrigin() ==
           web_frame()->GetSecurityOrigin();
  }

  // Return false as share-autofill is not allowed.
  return false;
}

bool AutofillDriverIOS::CanShowAutofillUi() const {
  return true;
}

base::flat_set<FieldGlobalId> AutofillDriverIOS::ApplyFormAction(
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    base::span<const FormFieldData> fields,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, FieldType>& field_type_map) {
  switch (action_type) {
    case mojom::FormActionType::kUndo:
      // TODO(crbug.com/40266549) Add Undo support on iOS.
      return {};
    case mojom::FormActionType::kFill: {
      auto callback = [](AutofillDriver& driver,
                         mojom::FormActionType action_type,
                         mojom::ActionPersistence action_persistence,
                         const std::vector<FormFieldData::FillData>& fields) {
        web::WebFrame* frame = cast(&driver)->web_frame();
        if (frame) {
          [cast(&driver)->bridge_ fillData:fields inFrame:frame];
        }
      };

      const url::Origin main_origin =
          client_->GetLastCommittedPrimaryMainFrameOrigin();
      if (IsAcrossIframesEnabled()) {
        return router_->ApplyFormAction(callback, action_type,
                                        action_persistence, fields, main_origin,
                                        triggered_origin, field_type_map);
      } else {
        std::vector<FieldGlobalId> safe_fields;
        for (const auto& field : fields) {
          safe_fields.push_back(field.global_id());
        }

        callback(
            *this, action_type, action_persistence,
            std::vector<FormFieldData::FillData>(fields.begin(), fields.end()));
        return safe_fields;
      }
    }
  }
}

void AutofillDriverIOS::ApplyFieldAction(
    mojom::FieldActionType action_type,
    mojom::ActionPersistence action_persistence,
    const FieldGlobalId& field_id,
    const std::u16string& value) {
  auto callback = [](AutofillDriver& driver, mojom::FieldActionType action_type,
                     mojom::ActionPersistence action_persistence,
                     FieldRendererId field, const std::u16string& value) {
    // For now, only support filling.
    switch (action_persistence) {
      case mojom::ActionPersistence::kFill: {
        [cast(&driver)->bridge_
            fillSpecificFormField:field
                        withValue:value
                          inFrame:cast(&driver)->web_frame()];
        break;
      }
      case mojom::ActionPersistence::kPreview:
        return;
    }
  };
  if (IsAcrossIframesEnabled()) {
    router_->ApplyFieldAction(callback, action_type, action_persistence,
                              field_id, value);
  } else {
    callback(*this, action_type, action_persistence, field_id.renderer_id,
             value);
  }
}

void AutofillDriverIOS::ExtractForm(
    FormGlobalId form,
    base::OnceCallback<void(AutofillDriver*, const std::optional<FormData>&)>
        response_callback) {
  // TODO(crbug.com/40284824): Implement ExtractForm().
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::SendTypePredictionsToRenderer(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  std::vector<FormDataPredictions> preds =
      FormStructure::GetFieldTypePredictions(forms);

  auto callback = [](AutofillDriver& driver,
                     const std::vector<FormDataPredictions>& preds) {
    web::WebFrame* frame = cast(&driver)->web_frame();
    if (!frame) {
      return;
    }
    [cast(&driver)->bridge_ fillFormDataPredictions:preds inFrame:frame];
  };

  if (IsAcrossIframesEnabled()) {
    router_->SendTypePredictionsToRenderer(callback, preds);
  } else {
    callback(*this, preds);
  }
}

void AutofillDriverIOS::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field_id,
    const std::u16string& value) {}

void AutofillDriverIOS::TriggerFormExtractionInDriverFrame(
    AutofillDriverRouterAndFormForestPassKey pass_key) {
  if (!is_processed()) {
    return;
  }
  [bridge_ scanFormsInWebState:web_state_ inFrame:web_frame()];
}

void AutofillDriverIOS::TriggerFormExtractionInAllFrames(
    base::OnceCallback<void(bool)> form_extraction_finished_callback) {
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::GetFourDigitCombinationsFromDom(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  // TODO(crbug.com/40260122): Implement GetFourDigitCombinationsFromDom() in
  // iOS.
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::RendererShouldClearPreviewedForm() {}

void AutofillDriverIOS::RendererShouldTriggerSuggestions(
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source) {
  // Triggering suggestions from the browser process is currently only used for
  // manual fallbacks on Desktop. It is not implemented on iOS.
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field_id,
    mojom::AutofillSuggestionAvailability suggestion_availability) {}

std::optional<net::IsolationInfo> AutofillDriverIOS::GetIsolationInfo() {
  // On iOS we have a single, shared URLLoaderFactory provided by BrowserState.
  // As it is shared, it is not trusted and we cannot assign trusted_params
  // to the network request. On iOS, the IsolationInfo should always be nullopt.
  return std::nullopt;
}

web::WebFrame* AutofillDriverIOS::web_frame() const {
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state_);
  return frames_manager->GetFrameWithId(web_frame_id_);
}

void AutofillDriverIOS::AskForValuesToFill(const FormData& form,
                                           const FieldGlobalId& field_id) {
  auto callback = [](AutofillDriver& driver, const FormData& form,
                     const FieldGlobalId& field_id,
                     const gfx::Rect& bounding_box,
                     AutofillSuggestionTriggerSource trigger_source) {
    driver.GetAutofillManager().OnAskForValuesToFill(
        form, field_id, bounding_box, trigger_source);
  };
  // The caret position is currently not extracted on iOS.
  gfx::Rect caret_bounds;
  if (IsAcrossIframesEnabled()) {
    // TODO(crbug.com/40269303): Distinguish between different trigger sources.
    router_->AskForValuesToFill(
        callback, *this, form, field_id, caret_bounds,
        autofill::AutofillSuggestionTriggerSource::kiOS);
  } else {
    callback(*this, form, field_id, caret_bounds,
             autofill::AutofillSuggestionTriggerSource::kiOS);
  }
}

void AutofillDriverIOS::DidFillAutofillFormData(const FormData& form,
                                                base::TimeTicks timestamp) {
  auto callback = [](AutofillDriver& driver, const FormData& form,
                     base::TimeTicks timestamp) {
    cast(&driver)->UpdateLastInteractedForm(/*form_data=*/form);
    driver.GetAutofillManager().OnDidFillAutofillFormData(form, timestamp);
  };
  if (IsAcrossIframesEnabled()) {
    router_->DidFillAutofillFormData(callback, *this, form, timestamp);
  } else {
    callback(*this, form, timestamp);
  }
}

void AutofillDriverIOS::FormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  auto callback = [](AutofillDriver& driver,
                     const std::vector<FormData>& updated_forms,
                     const std::vector<FormGlobalId>& removed_forms) {
    driver.GetAutofillManager().OnFormsSeen(updated_forms, removed_forms);
  };

  if (IsAcrossIframesEnabled()) {
    // Any RemoteFrameTokens encountered for the first time should be posted to
    // the registrar, which allows this driver to be established as the parent
    // of the child frame.
    for (const autofill::FormData& form : updated_forms) {
      for (const autofill::FrameTokenWithPredecessor& child_frame :
           form.child_frames()) {
        // This absl::get is safe because on iOS, FormData::child_frames is
        // only ever populated with RemoteFrameTokens. absl::get will fail a
        // CHECK if this assumption is ever wrong.
        auto token = absl::get<autofill::RemoteFrameToken>(child_frame.token);
        auto* registrar =
            ChildFrameRegistrar::GetOrCreateForWebState(web_state_);
        if (registrar && known_child_frames_.insert(token).second) {
          registrar->DeclareNewRemoteToken(
              token, base::BindOnce(&AutofillDriverIOS::SetSelfAsParent,
                                    weak_ptr_factory_.GetWeakPtr(), form));
        }
      }
    }
    router_->FormsSeen(callback, *this, updated_forms, removed_forms);
  } else {
    callback(*this, updated_forms, removed_forms);
  }
}

void AutofillDriverIOS::FormSubmitted(
    const FormData& form,
    bool known_success,
    mojom::SubmissionSource submission_source) {
  auto callback = [](AutofillDriver& driver, const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source) {
    base::UmaHistogramEnumeration(kAutofillSubmissionDetectionSourceHistogram,
                                  submission_source);
    driver.GetAutofillManager().OnFormSubmitted(form, known_success,
                                                submission_source);
    cast(&driver)->ClearLastInteractedForm();
  };
  if (IsAcrossIframesEnabled()) {
    router_->FormSubmitted(callback, *this, form, known_success,
                           submission_source);
  } else {
    callback(*this, form, known_success, submission_source);
  }
}

void AutofillDriverIOS::CaretMovedInFormField(const FormData& form,
                                              const FieldGlobalId& field_id,
                                              const gfx::Rect& caret_bounds) {
  GetAutofillManager().OnCaretMovedInFormField(form, field_id, caret_bounds);
}

void AutofillDriverIOS::TextFieldDidChange(const FormData& form,
                                           const FieldGlobalId& field_id,
                                           base::TimeTicks timestamp) {
  auto callback = [&](AutofillDriver& driver, const FormData& form,
                      const FieldGlobalId& field_global_id,
                      base::TimeTicks timestamp) {
    cast(&driver)->UpdateLastInteractedForm(
        /*form_data=*/form,
        /*formless_field=*/form.renderer_id() ? FieldRendererId()
                                              : field_global_id.renderer_id);
    driver.GetAutofillManager().OnTextFieldDidChange(form, field_id, timestamp);
  };

  if (IsAcrossIframesEnabled()) {
    router_->TextFieldDidChange(callback, *this, form, field_id, timestamp);
  } else {
    callback(*this, form, field_id, timestamp);
  }
}

void AutofillDriverIOS::SetParent(base::WeakPtr<AutofillDriverIOS> parent) {
  if (unregistered_) {
    // Do not set parent if the driver was unregistered to avoid any risk
    // of connecting it back into a form tree, where it has to be kept alone as
    // a standalone node.
    return;
  }

  parent_ = std::move(parent);
}

void AutofillDriverIOS::SetSelfAsParent(const autofill::FormData& form,
                                        LocalFrameToken token) {
  AutofillDriverIOS* child_driver =
      FromWebStateAndLocalFrameToken(web_state_, token);
  if (child_driver) {
    child_driver->SetParent(weak_ptr_factory_.GetWeakPtr());
  }
  // Redeclare the forms as seen to take into account the new parent to
  // establish the relation between the child frames and their host form in the
  // forms tree.
  auto callback = [](AutofillDriver& driver,
                     const std::vector<FormData>& updated_forms,
                     const std::vector<FormGlobalId>& removed_forms) {
    driver.GetAutofillManager().OnFormsSeen(updated_forms, removed_forms);
  };
  router_->FormsSeen(callback, *this, {form}, {});
}

void AutofillDriverIOS::UpdateLastInteractedForm(
    const FormData& form_data,
    const FieldRendererId& formless_field) {
  last_interacted_form_.emplace(form_data, formless_field);
}

void AutofillDriverIOS::ClearLastInteractedForm() {
  last_interacted_form_.reset();
}

void AutofillDriverIOS::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    LifecycleState old_state,
    LifecycleState new_state) {
  switch (new_state) {
    case LifecycleState::kInactive:
    case LifecycleState::kActive:
    case LifecycleState::kPendingReset:
      break;
    case LifecycleState::kPendingDeletion:
      manager_observation_.Reset();
      break;
  }
}

void AutofillDriverIOS::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  DCHECK_EQ(&manager, manager_.get());
  if (updated_forms.empty()) {
    return;
  }
  std::vector<raw_ptr<FormStructure, VectorExperimental>> form_structures;
  form_structures.reserve(updated_forms.size());
  for (const FormGlobalId& form : updated_forms) {
    if (FormStructure* form_structure = manager.FindCachedFormById(form)) {
      form_structures.push_back(form_structure);
    }
  }
  if (web::WebFrame* frame = web_frame()) {
    [bridge_ handleParsedForms:form_structures inFrame:frame];
  }
}

void AutofillDriverIOS::FormsRemoved(
    const std::set<FormRendererId>& removed_forms,
    const std::set<FieldRendererId>& removed_unowned_fields) {
  const bool submission_detected = DetectFormSubmissionAfterFormRemoval(
      removed_forms, removed_unowned_fields);
  RecordFormRemoval(
      submission_detected, /*removed_forms_count=*/removed_forms.size(),
      /*removed_unowned_fields_count=*/removed_unowned_fields.size());

  if (submission_detected) {
    UpdateLastInteractedFormFromFieldDataManager();

    FormSubmitted(last_interacted_form_->form_data,
                  /*known_success=*/true,
                  mojom::SubmissionSource::XHR_SUCCEEDED);
  }

  // Report the removed forms so Autofill can be synced with the DOM state.

  std::vector<FormGlobalId> forms_to_report =
      base::ToVector(removed_forms, [&](const auto& renderer_id) {
        return FormGlobalId{.frame_token = local_frame_token_,
                            .renderer_id = renderer_id};
      });

  if (!removed_unowned_fields.empty()) {
    // Determine whether the synthetic form should be reported as removed based
    // on the removed fields, where having all fields removed is considered as
    // a deletion.
    FormGlobalId synthetic_global_id = {.frame_token = local_frame_token_,
                                        .renderer_id = FormRendererId(0)};
    if (FormStructure* form =
            GetAutofillManager().FindCachedFormById(synthetic_global_id)) {
      std::set<FieldRendererId> form_fields;
      base::ranges::transform(form->fields(),
                              std::inserter(form_fields, form_fields.begin()),
                              [](const std::unique_ptr<AutofillField>& field) {
                                return field->renderer_id();
                              });
      // If the synthetic form fields are a subset of the removed fields, it
      // means that all the synthetic form fields were removed.
      const bool is_deleted =
          base::ranges::includes(removed_unowned_fields, form_fields);
      if (is_deleted) {
        forms_to_report.emplace_back(synthetic_global_id);
      }
    }

    // TODO(crbug.com/351685487): Trigger forms extraction if there are some
    // fields removed but not all of them.
  }

  if (!forms_to_report.empty()) {
    FormsSeen(/*updated_forms=*/{}, /*removed_forms=*/forms_to_report);
  }
}

bool AutofillDriverIOS::DetectFormSubmissionAfterFormRemoval(
    const std::set<FormRendererId>& removed_forms,
    const std::set<FieldRendererId>& removed_unowned_fields) const {
  // Detect a form submission only if the last interacted form or formless field
  // was removed.
  if (!last_interacted_form_) {
    return false;
  }

  const auto& last_interacted_form_id =
      last_interacted_form_->form_data.renderer_id();
  // Check if the last interacted form was removed.
  if (last_interacted_form_id &&
      removed_forms.find(last_interacted_form_id) != removed_forms.end()) {
    return true;
  }

  const auto& last_formless_field_id = last_interacted_form_->formless_field;

  // Check if the last interacted formless field was removed.
  return removed_unowned_fields.find(last_formless_field_id) !=
         removed_unowned_fields.end();
}

void AutofillDriverIOS::Unregister() {
  router_->UnregisterDriver(*this, /*driver_is_dying=*/true);
  unregistered_ = true;
}

void AutofillDriverIOS::UpdateLastInteractedFormFromFieldDataManager() {
  CHECK(last_interacted_form_);

  auto* frame = web_frame();
  if (!frame) {
    return;
  }

  FieldDataManager* field_data_manager =
      FieldDataManagerFactoryIOS::FromWebFrame(frame);

  // Update the snapshot of the last interacted form with the data in
  // FieldDataManager.
  std::vector<FormFieldData> fields =
      last_interacted_form_->form_data.ExtractFields();
  for (auto& field : fields) {
    const auto& field_id = field.renderer_id();
    if (!field_data_manager->HasFieldData(field_id)) {
      continue;
    }
    field.set_value(field_data_manager->GetUserInput(field_id));
    field.set_properties_mask(
        field_data_manager->GetFieldPropertiesMask(field_id));
  }
  last_interacted_form_->form_data.set_fields(std::move(fields));
}

void AutofillDriverIOS::RecordFormRemoval(bool submission_detected,
                                          int removed_forms_count,
                                          int removed_unowned_fields_count) {
  base::UmaHistogramBoolean(/*name=*/kFormSubmissionAfterFormRemovalHistogram,
                            /*sample=*/submission_detected);
  base::UmaHistogramCounts100(
      /*name=*/kFormRemovalRemovedUnownedFieldsHistogram,
      /*sample=*/removed_unowned_fields_count);

}

}  // namespace autofill
