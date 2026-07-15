// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/password_manager/password_manager_autofill_helper.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

PasswordManagerAutofillHelper::PasswordManagerAutofillHelper(
    AutofillClient* client)
    : client_(client) {}

PasswordManagerAutofillHelper::~PasswordManagerAutofillHelper() = default;

// static
bool PasswordManagerAutofillHelper::IsOtpFilledField(
    const AutofillField& field) {
  return field.last_modifier() == FieldModifier::kAutofill &&
         field.filling_product() == FillingProduct::kOneTimePassword;
}

bool PasswordManagerAutofillHelper::IsFieldFilledWithOtp(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  const AutofillManager* manager =
      client_->GetAutofillManagerForPrimaryMainFrame();
  if (!manager) {
    return false;
  }
  auto [form, field] = manager->FindFormAndField(form_id, field_id);
  return field && IsOtpFilledField(*field);
}

}  // namespace autofill
