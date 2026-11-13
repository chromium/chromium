// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change/change_password_generation_util.h"

#include <algorithm>
#include <cstdint>

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"

namespace password_manager {

namespace {

// Returns the password field in `form` whose requirements should be used for
// password generation, preferring the new password field over the current
// password field.
const autofill::FormFieldData* FindPasswordField(const PasswordForm& form) {
  for (autofill::FieldRendererId field_id :
       {form.new_password_element_renderer_id,
        form.password_element_renderer_id}) {
    if (!field_id) {
      continue;
    }
    auto iter = std::ranges::find(form.form_data.fields(), field_id,
                                  &autofill::FormFieldData::renderer_id);
    if (iter != form.form_data.fields().end()) {
      return &*iter;
    }
  }
  return nullptr;
}

}  // namespace

ChangePasswordGenerationResult GeneratePasswordForChangePasswordForm(
    const PasswordForm& form,
    PasswordGenerationFrameHelper& generation_helper) {
  const autofill::FormFieldData* password_field = FindPasswordField(form);
  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(form.form_data);
  // Without a password field only the domain-wide requirements apply.
  autofill::FieldSignature field_signature =
      password_field
          ? autofill::CalculateFieldSignatureForField(*password_field)
          : autofill::FieldSignature();
  uint64_t max_length = password_field ? password_field->max_length() : 0;

  ChangePasswordGenerationResult result;
  result.spec = generation_helper.GetPasswordRequirementsSpec(
      form.url,
      autofill::password_generation::PasswordGenerationType::kAutomatic,
      form_signature, field_signature, max_length);
  result.password = autofill::GeneratePassword(result.spec);
  return result;
}

}  // namespace password_manager
