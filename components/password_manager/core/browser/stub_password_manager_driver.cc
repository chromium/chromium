// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_driver.h"

namespace password_manager {

StubPasswordManagerDriver::StubPasswordManagerDriver() = default;
StubPasswordManagerDriver::~StubPasswordManagerDriver() = default;

int StubPasswordManagerDriver::GetId() const {
  return 0;
}

void StubPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
}

void StubPasswordManagerDriver::GeneratedPasswordAccepted(
    const std::u16string& password) {}

void StubPasswordManagerDriver::FillSuggestion(const std::u16string& username,
                                               const std::u16string& password) {
}

void StubPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {}

void StubPasswordManagerDriver::ClearPreviewedForm() {
}

PasswordGenerationFrameHelper*
StubPasswordManagerDriver::GetPasswordGenerationHelper() {
  return nullptr;
}

PasswordManager* StubPasswordManagerDriver::GetPasswordManager() {
  return nullptr;
}

PasswordAutofillManager*
StubPasswordManagerDriver::GetPasswordAutofillManager() {
  return nullptr;
}

autofill::AutofillDriver* StubPasswordManagerDriver::GetAutofillDriver() {
  return nullptr;
}

bool StubPasswordManagerDriver::IsMainFrame() const {
  return true;
}

bool StubPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

const GURL& StubPasswordManagerDriver::GetLastCommittedURL() const {
  return GURL::EmptyGURL();
}

}  // namespace password_manager
