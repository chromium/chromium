// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_driver_ios.h"

#import "base/memory/ptr_util.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/accessibility/ax_tree_id.h"
#import "ui/gfx/geometry/rect_f.h"

namespace autofill {

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

AutofillDriverIOS::AutofillDriverIOS(web::WebState* web_state,
                                     web::WebFrame* web_frame,
                                     AutofillClient* client,
                                     id<AutofillDriverIOSBridge> bridge,
                                     const std::string& app_locale)
    : web_state_(web_state),
      web_frame_id_(web_frame ? web_frame->GetFrameId() : ""),
      bridge_(bridge),
      client_(*client),
      manager_(std::make_unique<BrowserAutofillManager>(this, app_locale)) {
  manager_observation_.Observe(manager_.get());

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos)) {
    std::optional<base::UnguessableToken> token_temp =
        DeserializeJavaScriptFrameId(web_frame_id_);
    if (token_temp) {
      local_frame_token_ = LocalFrameToken(*token_temp);
    }
  }
}

AutofillDriverIOS::~AutofillDriverIOS() = default;

LocalFrameToken AutofillDriverIOS::GetFrameToken() const {
  return local_frame_token_;
}

std::optional<LocalFrameToken> AutofillDriverIOS::Resolve(FrameToken query) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos)) {
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
bool AutofillDriverIOS::IsInActiveFrame() const {
  return true;
}

bool AutofillDriverIOS::IsInAnyMainFrame() const {
  web::WebFrame* frame = web_frame();
  return frame ? frame->IsMainFrame() : true;
}

bool AutofillDriverIOS::IsPrerendering() const {
  return false;
}

bool AutofillDriverIOS::HasSharedAutofillPermission() const {
  return false;
}

bool AutofillDriverIOS::CanShowAutofillUi() const {
  return true;
}

base::flat_set<FieldGlobalId> AutofillDriverIOS::ApplyFormAction(
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, FieldType>& field_type_map) {
  switch (action_type) {
    case mojom::FormActionType::kUndo:
      // TODO(crbug.com/1441410) Add Undo support on iOS.
      return {};
    case mojom::FormActionType::kFill:
      web::WebFrame* frame = web_frame();
      if (frame) {
        [bridge_ fillFormData:data inFrame:frame];
      }
      std::vector<FieldGlobalId> safe_fields;
      for (const auto& field : data.fields) {
        safe_fields.push_back(field.global_id());
      }
      return safe_fields;
  }
}

void AutofillDriverIOS::ApplyFieldAction(
    mojom::FieldActionType action_type,
    mojom::ActionPersistence action_persistence,
    const FieldGlobalId& field,
    const std::u16string& value) {
  // For now, only support filling.
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill: {
      web::WebFrame* frame = web_frame();
      if (frame) {
        [bridge_ fillSpecificFormField:field.renderer_id
                             withValue:value
                               inFrame:frame];
      }
      break;
    }
    case mojom::ActionPersistence::kPreview:
      return;
  }
}

void AutofillDriverIOS::ExtractForm(
    FormGlobalId form,
    base::OnceCallback<void(AutofillDriver*, const std::optional<FormData>&)>
        response_callback) {
  // TODO(crbug.com/40284824): Implement ExtractForm().
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::SendAutofillTypePredictionsToRenderer(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  web::WebFrame* frame = web_frame();
  if (!frame) {
    return;
  }
  [bridge_ fillFormDataPredictions:FormStructure::GetFieldTypePredictions(forms)
                           inFrame:frame];
}

void AutofillDriverIOS::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field,
    const std::u16string& value) {}

void AutofillDriverIOS::TriggerFormExtractionInDriverFrame() {
  if (!is_processed()) {
    return;
  }
  [bridge_ scanFormsInWebState:web_state_ inFrame:web_frame()];
}

void AutofillDriverIOS::TriggerFormExtractionInAllFrames(
    base::OnceCallback<void(bool)> form_extraction_finished_callback) {
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::GetFourDigitCombinationsFromDOM(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  // TODO(crbug.com/40260122): Implement GetFourDigitCombinationsFromDOM in iOS.
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::RendererShouldClearFilledSection() {}

void AutofillDriverIOS::RendererShouldClearPreviewedForm() {
}

void AutofillDriverIOS::RendererShouldTriggerSuggestions(
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source) {
  // Triggering suggestions from the browser process is currently only used for
  // manual fallbacks on Desktop. It is not implemented on iOS.
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field,
    mojom::AutofillSuggestionAvailability suggestion_availability) {}

void AutofillDriverIOS::PopupHidden() {
}

net::IsolationInfo AutofillDriverIOS::IsolationInfo() {
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state_);
  web::WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  if (!main_web_frame)
    return net::IsolationInfo();

  web::WebFrame* frame = web_frame();
  if (!frame) {
    return net::IsolationInfo();
  }

  return net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      url::Origin::Create(main_web_frame->GetSecurityOrigin()),
      url::Origin::Create(frame->GetSecurityOrigin()), net::SiteForCookies());
}

web::WebFrame* AutofillDriverIOS::web_frame() const {
  web::WebFramesManager* frames_manager =
      AutofillJavaScriptFeature::GetInstance()->GetWebFramesManager(web_state_);
  return frames_manager->GetFrameWithId(web_frame_id_);
}

void AutofillDriverIOS::AskForValuesToFill(const FormData& form,
                                           const FormFieldData& field) {
  // TODO(crbug.com/1441921): Route this using AutofillDriverRouter.
  // TODO(crbug.com/40269303): Distinguish between different trigger sources.
  GetAutofillManager().OnAskForValuesToFill(
      form, field, /*bounding_box=*/gfx::RectF(),
      autofill::AutofillSuggestionTriggerSource::kiOS);
}

void AutofillDriverIOS::DidFillAutofillFormData(const FormData& form,
                                                base::TimeTicks timestamp) {
  // TODO(crbug.com/1441921): Route this using AutofillDriverRouter.
  GetAutofillManager().OnDidFillAutofillFormData(form, timestamp);
}

void AutofillDriverIOS::FormsSeen(const std::vector<FormData>& updated_forms) {
  // Any RemoteFrameTokens encountered for the first time should be posted to
  // the registrar, which allows this driver to be established as the parent of
  // the child frame.
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos)) {
    for (const autofill::FormData& form : updated_forms) {
      for (const autofill::FrameTokenWithPredecessor& child_frame :
           form.child_frames) {
        // This absl::get is safe because on iOS, FormData::child_frames is
        // only ever populated with RemoteFrameTokens. absl::get will fail a
        // CHECK if this assumption is ever wrong.
        auto token = absl::get<autofill::RemoteFrameToken>(child_frame.token);
        auto* registrar =
            ChildFrameRegistrar::GetOrCreateForWebState(web_state_);
        if (registrar && known_child_frames_.insert(token).second) {
          registrar->DeclareNewRemoteToken(
              token, base::BindOnce(&AutofillDriverIOS::SetSelfAsParent,
                                    weak_ptr_factory_.GetWeakPtr()));
        }
      }
    }
  }

  // TODO(crbug.com/1441921): Route this using AutofillDriverRouter.
  // TODO(crbug.com/40184363): Notify about deleted fields.
  GetAutofillManager().OnFormsSeen(updated_forms, {});
}

void AutofillDriverIOS::FormSubmitted(
    const FormData& form,
    bool known_success,
    mojom::SubmissionSource submission_source) {
  // TODO(crbug.com/1441921): Route this using AutofillDriverRouter.
  GetAutofillManager().OnFormSubmitted(form, known_success, submission_source);
}

void AutofillDriverIOS::TextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           base::TimeTicks timestamp) {
  // TODO(crbug.com/1441921): Route this using AutofillDriverRouter.
  GetAutofillManager().OnTextFieldDidChange(
      form, field,
      gfx::RectF(),  // Bounds aren't needed on iOS since we don't use popups.
      timestamp);
}

void AutofillDriverIOS::SetParent(base::WeakPtr<AutofillDriverIOS> parent) {
  parent_ = std::move(parent);
}

void AutofillDriverIOS::SetSelfAsParent(LocalFrameToken token) {
  AutofillDriverIOS* child_driver =
      FromWebStateAndLocalFrameToken(web_state_, token);
  if (child_driver) {
    child_driver->SetParent(weak_ptr_factory_.GetWeakPtr());
  }
}

void AutofillDriverIOS::OnAutofillManagerDestroyed(AutofillManager& manager) {
  manager_observation_.Reset();
}

void AutofillDriverIOS::OnAfterFormsSeen(AutofillManager& manager,
                                         base::span<const FormGlobalId> forms) {
  DCHECK_EQ(&manager, manager_.get());
  if (forms.empty()) {
    return;
  }
  std::vector<raw_ptr<FormStructure, VectorExperimental>> form_structures;
  form_structures.reserve(forms.size());
  for (const FormGlobalId& form : forms) {
    if (FormStructure* form_structure = manager.FindCachedFormById(form)) {
      form_structures.push_back(form_structure);
    }
  }
  if (web::WebFrame* frame = web_frame()) {
    [bridge_ handleParsedForms:form_structures inFrame:frame];
  }
}

}  // namespace autofill
