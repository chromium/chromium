// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/outgoing_tab_form_field_extractor.h"

#include <string>
#include <utility>

#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/origin.h"

namespace send_tab_to_self {

namespace {

bool HasRequiredAttributes(const autofill::AutofillField& field) {
  return !field.value().empty() &&
         (!field.id_attribute().empty() || !field.name_attribute().empty());
}

bool IsSensitiveFieldType(autofill::FormControlType type) {
  // Explicitly exclude password fields.
  switch (type) {
    case autofill::FormControlType::kInputPassword:
      return true;
    case autofill::FormControlType::kContentEditable:
    case autofill::FormControlType::kInputCheckbox:
    case autofill::FormControlType::kInputDate:
    case autofill::FormControlType::kInputEmail:
    case autofill::FormControlType::kInputMonth:
    case autofill::FormControlType::kInputNumber:
    case autofill::FormControlType::kInputRadio:
    case autofill::FormControlType::kInputSearch:
    case autofill::FormControlType::kInputTelephone:
    case autofill::FormControlType::kInputText:
    case autofill::FormControlType::kInputUrl:
    case autofill::FormControlType::kSelectOne:
    case autofill::FormControlType::kTextArea:
      return false;
  }

  NOTREACHED();
}

}  // namespace

PageContext::FormFieldInfo ExtractOutgoingTabFormFields(
    autofill::AutofillManager& manager,
    const url::Origin& origin) {
  PageContext::FormFieldInfo form_field_info;
  manager.ForEachCachedForm([&](const autofill::FormStructure& form) {
    for (const std::unique_ptr<autofill::AutofillField>& field :
         form.fields()) {
      if (field->origin() != origin) {
        // Only same-origin fields are considered for security reason.
        continue;
      }

      // Filter out form fields that the user didn't interact with.
      if (!field->all_modifiers().contains_any(
              {autofill::FieldModifier::kUser,
               autofill::FieldModifier::kAutofill})) {
        continue;
      }

      if (!HasRequiredAttributes(*field)) {
        continue;
      }

      if (IsSensitiveFieldType(field->form_control_type())) {
        continue;
      }

      PageContext::FormField field_data;
      field_data.id_attribute = field->id_attribute();
      field_data.name_attribute = field->name_attribute();
      field_data.form_control_type = std::string(
          autofill::FormControlTypeToString(field->form_control_type()));
      field_data.value = field->value();
      form_field_info.fields.push_back(std::move(field_data));
    }
  });
  return form_field_info;
}

}  // namespace send_tab_to_self
