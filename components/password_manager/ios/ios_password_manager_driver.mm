// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/ios_password_manager_driver.h"

#include <string>

#include "base/hash/hash.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordAutofillManager;
using password_manager::PasswordManager;

IOSPasswordManagerDriver::IOSPasswordManagerDriver(
    web::WebState* web_state,
    id<PasswordManagerDriverBridge> bridge,
    password_manager::PasswordManagerInterface* password_manager,
    web::WebFrame* web_frame,
    int driver_id)
    : web_state_(web_state->GetWeakPtr()),
      bridge_(bridge),
      password_manager_(password_manager),
      id_(driver_id),
      cached_frame_id_(base::FastHash(web_frame->GetFrameId())),
      frame_id_(web_frame->GetFrameId()) {
  password_generation_helper_ =
      std::make_unique<password_manager::PasswordGenerationFrameHelper>(
          password_manager_->GetClient(), this);

  // Cache these values early, so that it can be accessed after frame deletion.
  is_in_main_frame_ = web_frame->IsMainFrame();
  security_origin_ = web_frame->GetSecurityOrigin();
}

IOSPasswordManagerDriver::~IOSPasswordManagerDriver() = default;

int IOSPasswordManagerDriver::GetId() const {
  return id_;
}

void IOSPasswordManagerDriver::SetPasswordFillData(
    const autofill::PasswordFormFillData& form_data) {
  [bridge_ processPasswordFormFillData:form_data
                            forFrameId:frame_id_
                           isMainFrame:is_in_main_frame_
                     forSecurityOrigin:security_origin_];
}

void IOSPasswordManagerDriver::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  [bridge_ onNoSavedCredentialsWithFrameId:frame_id_];
}

void IOSPasswordManagerDriver::FormEligibleForGenerationFound(
    const autofill::PasswordFormGenerationData& form) {
  if (GetPasswordGenerationHelper() &&
      GetPasswordGenerationHelper()->IsGenerationEnabled(
          /*log_debug_data*/ true)) {
    [bridge_ formEligibleForGenerationFound:form];
  }
}

void IOSPasswordManagerDriver::GeneratedPasswordAccepted(
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSPasswordManagerDriver::FillSuggestion(const std::u16string& username,
                                              const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSPasswordManagerDriver::PreviewGenerationSuggestion(
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSPasswordManagerDriver::ClearPreviewedForm() {
  NOTIMPLEMENTED();
}

void IOSPasswordManagerDriver::SetSuggestionAvailability(
    autofill::FieldRendererId generation_element_id,
    const autofill::mojom::AutofillState state) {
  NOTIMPLEMENTED();
}

password_manager::PasswordGenerationFrameHelper*
IOSPasswordManagerDriver::GetPasswordGenerationHelper() {
  return password_generation_helper_.get();
}

password_manager::PasswordManagerInterface*
IOSPasswordManagerDriver::GetPasswordManager() {
  return password_manager_;
}

PasswordAutofillManager*
IOSPasswordManagerDriver::GetPasswordAutofillManager() {
  // TODO(crbug.com/341877): Use PasswordAutofillManager to implement password
  // autofill on iOS.
  return nullptr;
}

bool IOSPasswordManagerDriver::IsInPrimaryMainFrame() const {
  return is_in_main_frame_;
}

bool IOSPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

::ui::AXTreeID IOSPasswordManagerDriver::GetAxTreeId() const {
  return {};
}

int IOSPasswordManagerDriver::GetFrameId() const {
  return cached_frame_id_;
}

const GURL& IOSPasswordManagerDriver::GetLastCommittedURL() const {
  return bridge_.lastCommittedURL;
}
