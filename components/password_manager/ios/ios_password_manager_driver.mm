// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/ios_password_manager_driver.h"

#import <string>

#import "base/hash/hash.h"
#import "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/password_manager/core/browser/password_generation_frame_helper.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"

using password_manager::PasswordAutofillManager;
using password_manager::PasswordManager;

namespace {
// Maximal number of pending forms for proactive generation that can be queued.
constexpr int kMaxPendingFormsForProactiveGeneration = 10;
}  // namespace

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
      frame_id_(web_frame->GetFrameId()),
      field_data_manager_(
          autofill::FieldDataManagerFactoryIOS::GetRetainable(web_frame)) {
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
  // Disable proactive generation and clear the pending forms if it is known
  // that there are passwords available for the site. This signal won't work for
  // passwords added after the frame is loaded, TODO(crbug.com/316132527): fix
  // that.
  can_use_proactive_generation_ = false;
  pending_forms_for_proactive_generation_.clear();

  [bridge_ processPasswordFormFillData:form_data
                            forFrameId:frame_id_
                           isMainFrame:is_in_main_frame_
                     forSecurityOrigin:security_origin_];
}

void IOSPasswordManagerDriver::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  // Allow using the proactive password generation bottom sheet from now on
  // since it is now known that there are no credentials saved for this page.
  // This signal won't work if the passwords are removed after the frame is
  // loaded, TODO(crbug.com/316132527): fix that.
  can_use_proactive_generation_ = true;

  // Attach the listeners on forms that couldn't be processed yet.
  for (const auto& form : pending_forms_for_proactive_generation_) {
    [bridge_ attachListenersForPasswordGenerationFields:form
                                             forFrameId:frame_id_];
  }
  pending_forms_for_proactive_generation_.clear();

  [bridge_ onNoSavedCredentialsWithFrameId:frame_id_];
}

void IOSPasswordManagerDriver::FormEligibleForGenerationFound(
    const autofill::PasswordFormGenerationData& form) {
  if (GetPasswordGenerationHelper() &&
      GetPasswordGenerationHelper()->IsGenerationEnabled(
          /*log_debug_data*/ true)) {
    [bridge_ formEligibleForGenerationFound:form];
    if (can_use_proactive_generation_) {
      [bridge_ attachListenersForPasswordGenerationFields:form
                                               forFrameId:frame_id_];
    } else if (pending_forms_for_proactive_generation_.size() <
               kMaxPendingFormsForProactiveGeneration) {
      // Push processing the forms eligible for generation for later since it
      // isn't yet known whether the proactive generation can be used. Don't
      // push more than `kMaxPendingFormsForProactiveGeneration` to avoid
      // bloating memory in the case the queue is never cleaned up.
      pending_forms_for_proactive_generation_.push_back(form);
    }
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

void IOSPasswordManagerDriver::FillSuggestionById(
    autofill::FieldRendererId username_element_id,
    autofill::FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED() << "This function is used for non-iOS manual fallback";
}

void IOSPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSPasswordManagerDriver::PreviewSuggestionById(
    autofill::FieldRendererId username_element_id,
    autofill::FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED() << "This function is used for non-iOS manual fallback";
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
    autofill::mojom::AutofillSuggestionAvailability suggestion_availability) {
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
  // TODO(crbug.com/41088554): Use PasswordAutofillManager to implement password
  // autofill on iOS.
  return nullptr;
}

bool IOSPasswordManagerDriver::IsInPrimaryMainFrame() const {
  return is_in_main_frame_;
}

bool IOSPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

int IOSPasswordManagerDriver::GetFrameId() const {
  return cached_frame_id_;
}

const GURL& IOSPasswordManagerDriver::GetLastCommittedURL() const {
  return bridge_.lastCommittedURL;
}

base::WeakPtr<password_manager::PasswordManagerDriver>
IOSPasswordManagerDriver::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
