// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sensitive_content/sensitive_content_manager.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/sensitive_content/sensitive_content_client.h"
#include "content/public/browser/web_contents.h"

namespace sensitive_content {

namespace {

using autofill::AutofillField;
using autofill::AutofillManager;
using autofill::FieldType;
using autofill::FieldTypeGroup;
using autofill::FormGlobalId;

bool IsSensitiveAutofillType(FieldType type) {
  static constexpr autofill::FieldTypeSet kNonSensitivePasswordTypes = {
      FieldType::NOT_ACCOUNT_CREATION_PASSWORD, FieldType::NOT_NEW_PASSWORD,
      FieldType::NOT_PASSWORD, FieldType::NOT_USERNAME};
  FieldTypeGroup field_type_group = autofill::GroupTypeOfFieldType(type);
  // Return true if the field is a credit card or password form field.
  // A field is a password form field if it's part of
  // `FieldTypeGroup::kPasswordField`, but it's not prefixed with "NOT_".
  return field_type_group == FieldTypeGroup::kCreditCard ||
         field_type_group == FieldTypeGroup::kStandaloneCvcField ||
         (field_type_group == FieldTypeGroup::kPasswordField &&
          !kNonSensitivePasswordTypes.contains(type));
}

}  // namespace

SensitiveContentManager::SensitiveContentManager(
    content::WebContents* web_contents,
    SensitiveContentClient* client)
    : client_(CHECK_DEREF(client)) {
  autofill_managers_observation_.Observe(web_contents);
}

SensitiveContentManager::~SensitiveContentManager() = default;

void SensitiveContentManager::UpdateContentSensitivity() {
  const bool content_is_sensitive = !sensitive_fields_.empty();
  // Prevent unnecessary calls to the client.
  if (last_content_was_sensitive_ != content_is_sensitive) {
    client_->SetContentSensitivity(!sensitive_fields_.empty());
    last_content_was_sensitive_ = content_is_sensitive;
  }
}

void SensitiveContentManager::OnFieldTypesDetermined(AutofillManager& manager,
                                                     FormGlobalId form,
                                                     FieldTypeSource) {
  const std::vector<std::unique_ptr<AutofillField>>& fields =
      manager.FindCachedFormById(form)->fields();

  for (const std::unique_ptr<AutofillField>& field : fields) {
    if (IsSensitiveAutofillType(field->Type().GetStorableType())) {
      sensitive_fields_.insert(field->global_id());
    } else {
      sensitive_fields_.erase(field->global_id());
    }
  }
  UpdateContentSensitivity();
}

void SensitiveContentManager::OnBeforeFormsSeen(
    AutofillManager& manager,
    base::span<const autofill::FormGlobalId> updated_forms,
    base::span<const autofill::FormGlobalId> removed_forms) {
  for (const FormGlobalId& form_id : removed_forms) {
    const std::vector<std::unique_ptr<AutofillField>>& fields =
        manager.FindCachedFormById(form_id)->fields();
    for (const std::unique_ptr<AutofillField>& field : fields) {
      sensitive_fields_.erase(field->global_id());
    }
  }
  UpdateContentSensitivity();
}

}  // namespace sensitive_content
