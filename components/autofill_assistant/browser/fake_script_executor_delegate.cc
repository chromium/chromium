// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"

#include <utility>

#include "components/password_manager/core/browser/password_change_success_tracker.h"

namespace autofill_assistant {

FakeScriptExecutorDelegate::FakeScriptExecutorDelegate()
    : trigger_context_(std::make_unique<TriggerContext>()) {}

FakeScriptExecutorDelegate::~FakeScriptExecutorDelegate() = default;

const ClientSettings& FakeScriptExecutorDelegate::GetSettings() {
  return client_settings_;
}

const GURL& FakeScriptExecutorDelegate::GetCurrentURL() {
  return current_url_;
}

const GURL& FakeScriptExecutorDelegate::GetDeeplinkURL() {
  return current_url_;
}

const GURL& FakeScriptExecutorDelegate::GetScriptURL() {
  return current_url_;
}

Service* FakeScriptExecutorDelegate::GetService() {
  return service_;
}

WebController* FakeScriptExecutorDelegate::GetWebController() {
  return web_controller_;
}

bool FakeScriptExecutorDelegate::IsXmlSigned(
    const std::string& xml_string) const {
  return true;
}

const std::vector<std::string>
FakeScriptExecutorDelegate::ExtractValuesFromSingleTagXml(
    const std::string& xml_string,
    const std::vector<std::string>& keys) const {
  return (const std::vector<std::string>){};
}

TriggerContext* FakeScriptExecutorDelegate::GetTriggerContext() {
  return trigger_context_.get();
}

autofill::PersonalDataManager*
FakeScriptExecutorDelegate::GetPersonalDataManager() {
  return nullptr;
}

WebsiteLoginManager* FakeScriptExecutorDelegate::GetWebsiteLoginManager() {
  return nullptr;
}

password_manager::PasswordChangeSuccessTracker*
FakeScriptExecutorDelegate::GetPasswordChangeSuccessTracker() {
  return nullptr;
}

content::WebContents* FakeScriptExecutorDelegate::GetWebContents() {
  return web_contents_;
}

const std::string FakeScriptExecutorDelegate::GetLocale() {
  return "en-US";
}

void FakeScriptExecutorDelegate::SetJsFlowLibrary(
    const std::string& js_flow_library) {
  GetJsFlowDevtoolsWrapper()->SetJsFlowLibrary(js_flow_library);
}

JsFlowDevtoolsWrapper* FakeScriptExecutorDelegate::GetJsFlowDevtoolsWrapper() {
  if (!js_flow_devtools_wrapper_) {
    content::WebContents* web_contents = GetWebContents();
    DCHECK(web_contents_)
        << "devtools wrapper is only available in browsertests";

    js_flow_devtools_wrapper_ =
        std::make_unique<JsFlowDevtoolsWrapper>(web_contents);
  }

  return js_flow_devtools_wrapper_.get();
}

std::string FakeScriptExecutorDelegate::GetEmailAddressForAccessTokenAccount() {
  return std::string();
}

ukm::UkmRecorder* FakeScriptExecutorDelegate::GetUkmRecorder() {
  return nullptr;
}

bool FakeScriptExecutorDelegate::EnterState(AutofillAssistantState state) {
  if (GetState() == state)
    return false;

  state_history_.emplace_back(state);
  return true;
}

AutofillAssistantState FakeScriptExecutorDelegate::GetState() const {
  return state_history_.empty() ? AutofillAssistantState::INACTIVE
                                : state_history_.back();
}

void FakeScriptExecutorDelegate::SetTouchableElementArea(
    const ElementAreaProto& element_area) {
  touchable_element_area_history_.emplace_back(element_area);
}

void FakeScriptExecutorDelegate::WriteUserData(
    base::OnceCallback<void(UserData*, UserDataFieldChange*)> write_callback) {}

void FakeScriptExecutorDelegate::SetViewportMode(ViewportMode mode) {
  viewport_mode_ = mode;
}

ViewportMode FakeScriptExecutorDelegate::GetViewportMode() {
  return viewport_mode_;
}

void FakeScriptExecutorDelegate::ExpectNavigation() {}

bool FakeScriptExecutorDelegate::HasNavigationError() {
  return navigation_error_;
}

bool FakeScriptExecutorDelegate::IsNavigatingToNewDocument() {
  return navigating_to_new_document_;
}

void FakeScriptExecutorDelegate::RequireUI() {
  require_ui_ = true;
}

ProcessedActionStatusDetailsProto& FakeScriptExecutorDelegate::GetLogInfo() {
  return log_info_;
}

bool FakeScriptExecutorDelegate::MustUseBackendData() const {
  return must_use_backend_data_;
}

void FakeScriptExecutorDelegate::AddNavigationListener(
    ScriptExecutorDelegate::NavigationListener* listener) {
  navigation_listeners_.insert(listener);
}

void FakeScriptExecutorDelegate::RemoveNavigationListener(
    ScriptExecutorDelegate::NavigationListener* listener) {
  navigation_listeners_.erase(listener);
}

void FakeScriptExecutorDelegate::SetBrowseDomainsAllowlist(
    std::vector<std::string> domains) {
  browse_domains_ = std::move(domains);
}

void FakeScriptExecutorDelegate::SetClientSettings(
    const ClientSettingsProto& client_settings) {
  client_settings_.UpdateFromProto(client_settings);
}

UserModel* FakeScriptExecutorDelegate::GetUserModel() {
  return user_model_;
}

UserData* FakeScriptExecutorDelegate::GetUserData() {
  return user_data_;
}

void FakeScriptExecutorDelegate::SetOverlayBehavior(
    ConfigureUiStateProto::OverlayBehavior overaly_behavior) {}

void FakeScriptExecutorDelegate::SetBrowseModeInvisible(bool invisible) {}

bool FakeScriptExecutorDelegate::ShouldShowWarning() {
  return true;
}

std::vector<std::string>*
FakeScriptExecutorDelegate::GetCurrentBrowseDomainsList() {
  return &browse_domains_;
}

void FakeScriptExecutorDelegate::OnActionsResponseReceived(
    const RoundtripNetworkStats& network_stats) {
  roundtrip_network_stats_ = network_stats;
}

}  // namespace autofill_assistant
