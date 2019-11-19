// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"

#include <utility>

namespace autofill_assistant {

FakeScriptExecutorDelegate::FakeScriptExecutorDelegate()
    : trigger_context_(TriggerContext::CreateEmpty()) {}

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

Service* FakeScriptExecutorDelegate::GetService() {
  return service_;
}

WebController* FakeScriptExecutorDelegate::GetWebController() {
  return web_controller_;
}

ClientMemory* FakeScriptExecutorDelegate::GetClientMemory() {
  return &memory_;
}

TriggerContext* FakeScriptExecutorDelegate::GetTriggerContext() {
  return trigger_context_.get();
}

autofill::PersonalDataManager*
FakeScriptExecutorDelegate::GetPersonalDataManager() {
  return nullptr;
}

WebsiteLoginFetcher* FakeScriptExecutorDelegate::GetWebsiteLoginFetcher() {
  return nullptr;
}

content::WebContents* FakeScriptExecutorDelegate::GetWebContents() {
  return nullptr;
}

std::string FakeScriptExecutorDelegate::GetAccountEmailAddress() {
  return std::string();
}

void FakeScriptExecutorDelegate::EnterState(AutofillAssistantState state) {
  state_ = state;
}

void FakeScriptExecutorDelegate::SetTouchableElementArea(
    const ElementAreaProto& element) {}

void FakeScriptExecutorDelegate::SetStatusMessage(const std::string& message) {
  status_message_ = message;
}

std::string FakeScriptExecutorDelegate::GetStatusMessage() const {
  return status_message_;
}

void FakeScriptExecutorDelegate::SetBubbleMessage(const std::string& message) {
  status_message_ = message;
}

std::string FakeScriptExecutorDelegate::GetBubbleMessage() const {
  return status_message_;
}

void FakeScriptExecutorDelegate::SetDetails(std::unique_ptr<Details> details) {
  details_ = std::move(details);
}

void FakeScriptExecutorDelegate::SetInfoBox(const InfoBox& info_box) {
  info_box_ = std::make_unique<InfoBox>(info_box);
}

void FakeScriptExecutorDelegate::ClearInfoBox() {
  info_box_ = nullptr;
}

void FakeScriptExecutorDelegate::SetProgress(int progress) {}

void FakeScriptExecutorDelegate::SetProgressVisible(bool visible) {}

void FakeScriptExecutorDelegate::SetUserActions(
    std::unique_ptr<std::vector<UserAction>> user_actions) {
  user_actions_ = std::move(user_actions);
}

void FakeScriptExecutorDelegate::SetCollectUserDataOptions(
    std::unique_ptr<CollectUserDataOptions> options,
    std::unique_ptr<UserData> information) {
  payment_request_options_ = std::move(options);
  payment_request_info_ = std::move(information);
}

void FakeScriptExecutorDelegate::WriteUserData(
    base::OnceCallback<void(const CollectUserDataOptions*,
                            UserData*,
                            UserData::FieldChange*)> write_callback) {
  if (payment_request_options_ == nullptr || payment_request_info_ == nullptr) {
    return;
  }

  UserData::FieldChange field_change = UserData::FieldChange::NONE;
  std::move(write_callback)
      .Run(payment_request_options_.get(), payment_request_info_.get(),
           &field_change);
}

void FakeScriptExecutorDelegate::SetViewportMode(ViewportMode mode) {
  viewport_mode_ = mode;
}

ViewportMode FakeScriptExecutorDelegate::GetViewportMode() {
  return viewport_mode_;
}

void FakeScriptExecutorDelegate::SetPeekMode(
    ConfigureBottomSheetProto::PeekMode peek_mode) {
  peek_mode_ = peek_mode;
}

ConfigureBottomSheetProto::PeekMode FakeScriptExecutorDelegate::GetPeekMode() {
  return peek_mode_;
}

bool FakeScriptExecutorDelegate::HasNavigationError() {
  return navigation_error_;
}

bool FakeScriptExecutorDelegate::IsNavigatingToNewDocument() {
  return navigating_to_new_document_;
}

void FakeScriptExecutorDelegate::RequireUI() {
  require_ui_ = true;
}

void FakeScriptExecutorDelegate::AddListener(Listener* listener) {
  listeners_.insert(listener);
}

void FakeScriptExecutorDelegate::RemoveListener(Listener* listener) {
  listeners_.erase(listener);
}

bool FakeScriptExecutorDelegate::SetForm(
    std::unique_ptr<FormProto> form,
    base::RepeatingCallback<void(const FormProto::Result*)> callback) {
  return true;
}
}  // namespace autofill_assistant
