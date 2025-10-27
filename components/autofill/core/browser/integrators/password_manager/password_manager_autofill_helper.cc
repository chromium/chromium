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

bool PasswordManagerAutofillHelper::IsFieldFilledWithOtp(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  AutofillManager* manager = client_->GetAutofillManagerForPrimaryMainFrame();
  if (!manager) {
    return false;
  }

  if (FormStructure* form = manager->FindCachedFormById(form_id)) {
    if (AutofillField* field = form->GetFieldById(field_id)) {
      return field->filling_product() == FillingProduct::kOneTimePassword;
    }
  }
  return false;
}

}  // namespace autofill
