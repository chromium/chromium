// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/ios_password_manager_driver.h"

#include <string>

#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordAutofillManager;
using password_manager::PasswordManager;

IOSPasswordManagerDriver::IOSPasswordManagerDriver(
    id<PasswordManagerDriverBridge> bridge,
    password_manager::PasswordManager* password_manager)
    : bridge_(bridge), password_manager_(password_manager) {
  password_generation_helper_ =
      std::make_unique<password_manager::PasswordGenerationFrameHelper>(
          password_manager_->client(), this);
}

IOSPasswordManagerDriver::~IOSPasswordManagerDriver() = default;

int IOSPasswordManagerDriver::GetId() const {
  // There is only one driver per tab on iOS so returning 0 is fine.
  return 0;
}

void IOSPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  [bridge_ fillPasswordForm:form_data completionHandler:nil];
}

void IOSPasswordManagerDriver::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  [bridge_ onNoSavedCredentials];
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

void IOSPasswordManagerDriver::ClearPreviewedForm() {
  NOTIMPLEMENTED();
}

password_manager::PasswordGenerationFrameHelper*
IOSPasswordManagerDriver::GetPasswordGenerationHelper() {
  return password_generation_helper_.get();
}

PasswordManager* IOSPasswordManagerDriver::GetPasswordManager() {
  return password_manager_;
}

PasswordAutofillManager*
IOSPasswordManagerDriver::GetPasswordAutofillManager() {
  // TODO(crbug.com/341877): Use PasswordAutofillManager to implement password
  // autofill on iOS.
  return nullptr;
}

bool IOSPasswordManagerDriver::IsInPrimaryMainFrame() const {
  // On IOS only processing of password forms in main frame is implemented.
  return true;
}

bool IOSPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

::ui::AXTreeID IOSPasswordManagerDriver::GetAxTreeId() const {
  return {};
}

const GURL& IOSPasswordManagerDriver::GetLastCommittedURL() const {
  return bridge_.lastCommittedURL;
}
