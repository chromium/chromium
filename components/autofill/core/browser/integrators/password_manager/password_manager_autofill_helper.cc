// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/password_manager/password_manager_autofill_helper.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"

namespace autofill {

PasswordManagerAutofillHelper::PasswordManagerAutofillHelper(
    AutofillClient* client)
    : client_(client) {}

PasswordManagerAutofillHelper::~PasswordManagerAutofillHelper() = default;

namespace {

AutofillField* GetAutofillField(AutofillManager* manager,
                                FormGlobalId form_id,
                                FieldGlobalId field_id) {
  if (!manager) {
    return nullptr;
  }
  FormStructure* form = manager->FindCachedFormById(form_id);
  if (!form) {
    return nullptr;
  }
  return form->GetFieldById(field_id);
}

}  // namespace

// static
bool PasswordManagerAutofillHelper::IsOtpFilledField(
    const AutofillField& field) {
  return field.is_autofilled() &&
         field.filling_product() == FillingProduct::kOneTimePassword;
}

bool PasswordManagerAutofillHelper::IsFieldFilledWithOtp(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  AutofillManager* manager = client_->GetAutofillManagerForPrimaryMainFrame();
  AutofillField* field = GetAutofillField(manager, form_id, field_id);
  return field && IsOtpFilledField(*field);
}

}  // namespace autofill
