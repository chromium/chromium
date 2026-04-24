// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/outgoing_tab_form_field_extractor.h"

#include <ostream>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_ostream_operators.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/origin.h"

namespace send_tab_to_self {

namespace {

enum class ExtractionResult {
  kSuccess,
  kCrossOrigin,
  kNoIdOrName,
  kEmptyValue,
  kNoUserInteraction,
  kSensitiveType,
};

void LogFieldExtraction(std::ostream* os,
                        const autofill::AutofillField& field,
                        ExtractionResult result) {
  if (!os) {
    return;
  }
  CHECK_IS_TEST();
  *os << "Field [id='" << field.id_attribute() << "', name='"
      << field.name_attribute() << "']: ";
  switch (result) {
    case ExtractionResult::kSuccess:
      *os << "EXTRACTED (value='" << field.value() << "')";
      break;
    case ExtractionResult::kCrossOrigin:
      *os << "REJECTED (cross-origin)";
      break;
    case ExtractionResult::kNoUserInteraction:
      *os << "REJECTED (no user interaction)";
      break;
    case ExtractionResult::kEmptyValue:
      *os << "REJECTED (empty value)";
      break;
    case ExtractionResult::kNoIdOrName:
      *os << "REJECTED (no id or name)";
      break;
    case ExtractionResult::kSensitiveType:
      *os << "REJECTED (sensitive type "
          << autofill::FormControlTypeToString(field.form_control_type())
          << ")";
      break;
  }
  *os << "\n";
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

ExtractionResult GetExtractionResult(const autofill::AutofillField& field,
                                     const url::Origin& origin) {
  if (field.origin() != origin) {
    return ExtractionResult::kCrossOrigin;
  }
  if (field.id_attribute().empty() && field.name_attribute().empty()) {
    return ExtractionResult::kNoIdOrName;
  }
  if (field.value().empty()) {
    return ExtractionResult::kEmptyValue;
  }
  if (!field.all_modifiers().contains_any(
          {autofill::FieldModifier::kUser,
           autofill::FieldModifier::kAutofill})) {
    return ExtractionResult::kNoUserInteraction;
  }
  if (IsSensitiveFieldType(field.form_control_type())) {
    return ExtractionResult::kSensitiveType;
  }
  return ExtractionResult::kSuccess;
}

PageContext::FormFieldInfo ExtractOutgoingTabFormFieldsInternal(
    autofill::AutofillManager& manager,
    const url::Origin& origin,
    std::ostream* os) {
  PageContext::FormFieldInfo form_field_info;
  manager.ForEachCachedForm([&](const autofill::FormStructure& form) {
    for (const std::unique_ptr<autofill::AutofillField>& field :
         form.fields()) {
      ExtractionResult result = GetExtractionResult(*field, origin);
      LogFieldExtraction(os, *field, result);

      if (result == ExtractionResult::kSuccess) {
        PageContext::FormField field_data;
        field_data.id_attribute = field->id_attribute();
        field_data.name_attribute = field->name_attribute();
        field_data.form_control_type = std::string(
            autofill::FormControlTypeToString(field->form_control_type()));
        field_data.value = field->value();
        field_data.autofill_signature.form_signature = form.form_signature();
        field_data.autofill_signature.field_signature =
            field->GetFieldSignature();
        form_field_info.fields.push_back(std::move(field_data));
      }
    }
  });
  return form_field_info;
}

}  // namespace

PageContext::FormFieldInfo ExtractOutgoingTabFormFields(
    autofill::AutofillManager& manager,
    const url::Origin& origin) {
  return ExtractOutgoingTabFormFieldsInternal(manager, origin, /*os=*/nullptr);
}

PageContext::FormFieldInfo ExtractOutgoingTabFormFieldsForTesting(  // IN-TEST
    autofill::AutofillManager& manager,
    const url::Origin& origin,
    std::ostream& os) {
  return ExtractOutgoingTabFormFieldsInternal(manager, origin, &os);
}

}  // namespace send_tab_to_self
