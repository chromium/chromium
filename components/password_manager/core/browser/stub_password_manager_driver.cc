// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_driver.h"

#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_field_data.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

StubPasswordManagerDriver::StubPasswordManagerDriver() = default;
StubPasswordManagerDriver::~StubPasswordManagerDriver() = default;

int StubPasswordManagerDriver::GetId() const {
  return 0;
}

void StubPasswordManagerDriver::PropagateFillDataOnParsingCompletion(
    const autofill::PasswordFormFillData& form_data) {}

void StubPasswordManagerDriver::GeneratedPasswordAccepted(
    const std::u16string& password) {}

void StubPasswordManagerDriver::GeneratedPasswordRejected() {}

void StubPasswordManagerDriver::FocusNextFieldAfterPasswords() {}

void StubPasswordManagerDriver::FillField(
    autofill::FieldRendererId triggering_field_id,
    const std::u16string& value,
    autofill::FieldPropertiesFlags field_properties,
    base::OnceCallback<void(bool)> success_callback) {}

void StubPasswordManagerDriver::FillSuggestion(
    const std::u16string& username,
    const std::u16string& password,
    base::OnceCallback<void(bool)> success_callback) {}

void StubPasswordManagerDriver::FillSuggestionById(
    autofill::FieldRendererId username_element_id,
    autofill::FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password,
    autofill::AutofillSuggestionTriggerSource suggestion_source) {}

#if BUILDFLAG(IS_ANDROID)
void StubPasswordManagerDriver::TriggerFormSubmission() {}
#endif

void StubPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {}

void StubPasswordManagerDriver::PreviewSuggestionById(
    autofill::FieldRendererId username_element_id,
    autofill::FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password) {}

void StubPasswordManagerDriver::PreviewGenerationSuggestion(
    const std::u16string& password) {}

void StubPasswordManagerDriver::ClearPreviewedForm() {}

void StubPasswordManagerDriver::SetSuggestionAvailability(
    autofill::FieldRendererId generation_element_id,
    autofill::mojom::AutofillSuggestionAvailability suggestion_availability) {}

PasswordGenerationFrameHelper*
StubPasswordManagerDriver::GetPasswordGenerationHelper() {
  return nullptr;
}

PasswordManagerInterface* StubPasswordManagerDriver::GetPasswordManager() {
  return nullptr;
}

PasswordAutofillManager*
StubPasswordManagerDriver::GetPasswordAutofillManager() {
  return nullptr;
}

bool StubPasswordManagerDriver::IsDirectChildOfPrimaryMainFrame() const {
  return false;
}

bool StubPasswordManagerDriver::IsInPrimaryMainFrame() const {
  return true;
}

bool StubPasswordManagerDriver::IsNestedWithinFencedFrame() const {
  return false;
}

bool StubPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

int StubPasswordManagerDriver::GetFrameId() const {
  return GetId();
}

const GURL& StubPasswordManagerDriver::GetLastCommittedURL() const {
  return GURL::EmptyGURL();
}

const url::Origin& StubPasswordManagerDriver::GetLastCommittedOrigin() const {
  return opaque_origin_;
}

gfx::RectF StubPasswordManagerDriver::TransformToRootCoordinates(
    const gfx::RectF& bounds_in_frame_coordinates) {
  return gfx::RectF();
}

void StubPasswordManagerDriver::CheckViewAreaVisible(
    autofill::FieldRendererId field_id,
    base::OnceCallback<void(bool)>) {}

autofill::AutofillDriver* StubPasswordManagerDriver::GetAutofillDriver() const {
  return nullptr;
}

base::WeakPtr<PasswordManagerDriver> StubPasswordManagerDriver::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace password_manager
