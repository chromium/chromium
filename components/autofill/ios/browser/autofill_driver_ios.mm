// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#include "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

// static
AutofillDriverIOS* AutofillDriverIOS::FromWebStateAndWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  return AutofillDriverIOSFactory::FromWebState(web_state)->DriverForFrame(
      web_frame);
}

AutofillDriverIOS::AutofillDriverIOS(web::WebState* web_state,
                                     web::WebFrame* web_frame,
                                     AutofillClient* client,
                                     id<AutofillDriverIOSBridge> bridge,
                                     const std::string& app_locale)
    : web_state_(web_state),
      bridge_(bridge),
      client_(client),
      browser_autofill_manager_(
          std::make_unique<BrowserAutofillManager>(this, client, app_locale)) {
  web_frame_id_ = web_frame ? web_frame->GetFrameId() : "";
}

AutofillDriverIOS::~AutofillDriverIOS() = default;

LocalFrameToken AutofillDriverIOS::GetFrameToken() const {
  NOTIMPLEMENTED();  // TODO(crbug.com/1441921) implement.
  return LocalFrameToken();
}

AutofillDriverIOS* AutofillDriverIOS::GetParent() {
  NOTIMPLEMENTED();  // TODO(crbug.com/1441921) implement.
  return nullptr;
}

absl::optional<LocalFrameToken> AutofillDriverIOS::Resolve(FrameToken query) {
  NOTIMPLEMENTED();  // TODO(crbug.com/1441921) implement.
  return absl::nullopt;
}

// Return true as iOS has no MPArch.
bool AutofillDriverIOS::IsInActiveFrame() const {
  return true;
}

bool AutofillDriverIOS::IsInAnyMainFrame() const {
  web::WebFrame* frame = web_frame();
  return frame ? frame->IsMainFrame() : true;
}

// iOS has no fenced frames.
bool AutofillDriverIOS::IsInFencedFrameRoot() const {
  return false;
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

ui::AXTreeID AutofillDriverIOS::GetAxTreeId() const {
  NOTIMPLEMENTED() << "See https://crbug.com/985933";
  return ui::AXTreeIDUnknown();
}

bool AutofillDriverIOS::RendererIsAvailable() {
  return true;
}

std::vector<FieldGlobalId> AutofillDriverIOS::FillOrPreviewForm(
    mojom::RendererFormDataAction action,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) {
  web::WebFrame* frame = web_frame();
  if (frame) {
    [bridge_ fillFormData:data inFrame:frame];
  }
  std::vector<FieldGlobalId> safe_fields;
  for (const auto& field : data.fields)
    safe_fields.push_back(field.global_id());
  return safe_fields;
}

void AutofillDriverIOS::HandleParsedForms(const std::vector<FormData>& forms) {
  const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& map =
      browser_autofill_manager_->form_structures();
  std::vector<FormStructure*> form_structures;
  form_structures.reserve(forms.size());
  for (const FormData& form : forms) {
    auto it = map.find(form.global_id());
    if (it != map.end())
      form_structures.push_back(it->second.get());
  }

  web::WebFrame* frame = web_frame();
  if (!frame) {
    return;
  }
  [bridge_ handleParsedForms:form_structures inFrame:frame];
}

void AutofillDriverIOS::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
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

void AutofillDriverIOS::SendFieldsEligibleForManualFillingToRenderer(
    const std::vector<FieldGlobalId>& fields) {}

void AutofillDriverIOS::SetShouldSuppressKeyboard(bool suppress) {
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::TriggerReparse() {
  NOTIMPLEMENTED();  // TODO(crbug.com/1441921) implement.
}

void AutofillDriverIOS::TriggerReparseInAllFrames(
    base::OnceCallback<void(bool)> trigger_reparse_finished_callback) {
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::GetFourDigitCombinationsFromDOM(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  // TODO(crbug.com/1423605): Implement GetFourDigitCombinationsFromDOM in iOS.
  NOTIMPLEMENTED();
}

void AutofillDriverIOS::RendererShouldClearFilledSection() {}

void AutofillDriverIOS::RendererShouldClearPreviewedForm() {
}

void AutofillDriverIOS::RendererShouldFillFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {}

void AutofillDriverIOS::RendererShouldPreviewFieldWithValue(
    const FieldGlobalId& field,
    const std::u16string& value) {}

void AutofillDriverIOS::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field,
    const mojom::AutofillState state) {}

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

}  // namespace autofill
