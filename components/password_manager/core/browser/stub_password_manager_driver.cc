// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_driver.h"

#include "url/gurl.h"

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

#if BUILDFLAG(IS_ANDROID)
void StubPasswordManagerDriver::TriggerFormSubmission() {}
#endif

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

bool StubPasswordManagerDriver::IsInPrimaryMainFrame() const {
  return true;
}

bool StubPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

::ui::AXTreeID StubPasswordManagerDriver::GetAxTreeId() const {
  return {};
}

const GURL& StubPasswordManagerDriver::GetLastCommittedURL() const {
  return GURL::EmptyGURL();
}

}  // namespace password_manager
