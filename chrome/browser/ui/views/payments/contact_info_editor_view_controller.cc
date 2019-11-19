// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/contact_info_editor_view_controller.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"

namespace payments {

ContactInfoEditorViewController::ContactInfoEditorViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    BackNavigationType back_navigation_type,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::AutofillProfile&)> on_added,
    autofill::AutofillProfile* profile,
    bool is_incognito)
    : EditorViewController(spec,
                           state,
                           dialog,
                           back_navigation_type,
                           is_incognito),
      profile_to_edit_(profile),
      on_edited_(std::move(on_edited)),
      on_added_(std::move(on_added)) {}

ContactInfoEditorViewController::~ContactInfoEditorViewController() {}

bool ContactInfoEditorViewController::IsEditingExistingItem() {
  return !!profile_to_edit_;
}

std::vector<EditorField>
ContactInfoEditorViewController::GetFieldDefinitions() {
  std::vector<EditorField> fields;
  if (spec()->request_payer_name()) {
    fields.push_back(EditorField(
        autofill::NAME_FULL,
        l10n_util::GetStringUTF16(IDS_PAYMENTS_NAME_FIELD_IN_CONTACT_DETAILS),
        EditorField::LengthHint::HINT_SHORT, /*required=*/true));
  }
  if (spec()->request_payer_phone()) {
    fields.push_back(EditorField(
        autofill::PHONE_HOME_WHOLE_NUMBER,
        l10n_util::GetStringUTF16(IDS_PAYMENTS_PHONE_FIELD_IN_CONTACT_DETAILS),
        EditorField::LengthHint::HINT_SHORT, /*required=*/true,
        EditorField::ControlType::TEXTFIELD_NUMBER));
  }
  if (spec()->request_payer_email()) {
    fields.push_back(EditorField(
        autofill::EMAIL_ADDRESS,
        l10n_util::GetStringUTF16(IDS_PAYMENTS_EMAIL_FIELD_IN_CONTACT_DETAILS),
        EditorField::LengthHint::HINT_SHORT, /*required=*/true));
  }
  return fields;
}

base::string16 ContactInfoEditorViewController::GetInitialValueForType(
    autofill::ServerFieldType type) {
  if (!profile_to_edit_)
    return base::string16();
  return GetValueForType(*profile_to_edit_, type);

  if (type == autofill::PHONE_HOME_WHOLE_NUMBER) {
    return autofill::i18n::GetFormattedPhoneNumberForDisplay(
        *profile_to_edit_, state()->GetApplicationLocale());
  }

  return profile_to_edit_->GetInfo(type, state()->GetApplicationLocale());
}

bool ContactInfoEditorViewController::ValidateModelAndSave() {
  // TODO(crbug.com/712224): Move this method and its helpers to a base class
  // shared with the Shipping Address editor.
  if (!ValidateInputFields())
    return false;

  if (profile_to_edit_) {
    PopulateProfile(profile_to_edit_);
    if (!is_incognito())
      state()->GetPersonalDataManager()->UpdateProfile(*profile_to_edit_);
    state()->profile_comparator()->Invalidate(*profile_to_edit_);
    std::move(on_edited_).Run();
    on_added_.Reset();
  } else {
    std::unique_ptr<autofill::AutofillProfile> profile =
        std::make_unique<autofill::AutofillProfile>();
    PopulateProfile(profile.get());
    if (!is_incognito())
      state()->GetPersonalDataManager()->AddProfile(*profile);
    std::move(on_added_).Run(*profile);
    on_edited_.Reset();
  }
  return true;
}

std::unique_ptr<ValidationDelegate>
ContactInfoEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  return std::make_unique<ContactInfoValidationDelegate>(
      field, state()->GetApplicationLocale(), this);
}

std::unique_ptr<ui::ComboboxModel>
ContactInfoEditorViewController::GetComboboxModelForType(
    const autofill::ServerFieldType& type) {
  NOTREACHED();
  return nullptr;
}

base::string16 ContactInfoEditorViewController::GetSheetTitle() {
  // TODO(crbug.com/712074): Title should reflect the missing information, if
  // applicable.
  return profile_to_edit_ ? l10n_util::GetStringUTF16(
                                IDS_PAYMENTS_EDIT_CONTACT_DETAILS_LABEL)
                          : l10n_util::GetStringUTF16(
                                IDS_PAYMENTS_ADD_CONTACT_DETAILS_LABEL);
}

void ContactInfoEditorViewController::PopulateProfile(
    autofill::AutofillProfile* profile) {
  for (const auto& field : text_fields()) {
    profile->SetInfo(autofill::AutofillType(field.second.type),
                     field.first->GetText(), state()->GetApplicationLocale());
  }
  profile->set_origin(autofill::kSettingsOrigin);
}

bool ContactInfoEditorViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::CONTACT_INFO_EDITOR_SHEET;
  return true;
}

base::string16 ContactInfoEditorViewController::GetValueForType(
    const autofill::AutofillProfile& profile,
    autofill::ServerFieldType type) {
  if (type == autofill::PHONE_HOME_WHOLE_NUMBER) {
    return autofill::i18n::GetFormattedPhoneNumberForDisplay(
        profile, state()->GetApplicationLocale());
  }

  return profile.GetInfo(type, state()->GetApplicationLocale());
}

ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ContactInfoValidationDelegate(const EditorField& field,
                                  const std::string& locale,
                                  ContactInfoEditorViewController* controller)
    : field_(field), controller_(controller), locale_(locale) {}

ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ~ContactInfoValidationDelegate() {}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ShouldFormat() {
  return field_.type == autofill::PHONE_HOME_WHOLE_NUMBER;
}

base::string16
ContactInfoEditorViewController::ContactInfoValidationDelegate::Format(
    const base::string16& text) {
  return base::UTF8ToUTF16(autofill::i18n::FormatPhoneForDisplay(
      base::UTF16ToUTF8(text),
      autofill::AutofillCountry::CountryCodeForLocale(locale_)));
}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    IsValidTextfield(views::Textfield* textfield,
                     base::string16* error_message) {
  return ValidateTextfield(textfield, error_message);
}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    TextfieldValueChanged(views::Textfield* textfield, bool was_blurred) {
  if (!was_blurred)
    return true;

  base::string16 error_message;
  bool is_valid = ValidateTextfield(textfield, &error_message);
  controller_->DisplayErrorMessageForField(field_.type, error_message);
  return is_valid;
}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ValidateTextfield(views::Textfield* textfield,
                      base::string16* error_message) {
  bool is_valid = true;

  // Show errors from merchant's retry() call.
  autofill::AutofillProfile* invalid_contact_profile =
      controller_->state()->invalid_contact_profile();
  if (invalid_contact_profile && error_message &&
      textfield->GetText() ==
          controller_->GetValueForType(*invalid_contact_profile, field_.type)) {
    *error_message = controller_->spec()->GetPayerError(field_.type);
    if (!error_message->empty())
      return false;
  }

  if (textfield->GetText().empty()) {
    is_valid = false;
    if (error_message) {
      *error_message = l10n_util::GetStringUTF16(
          IDS_PREF_EDIT_DIALOG_FIELD_REQUIRED_VALIDATION_MESSAGE);
    }
  } else {
    switch (field_.type) {
      case autofill::PHONE_HOME_WHOLE_NUMBER: {
        const std::string default_region_code =
            autofill::AutofillCountry::CountryCodeForLocale(locale_);
        if (!autofill::IsPossiblePhoneNumber(textfield->GetText(),
                                             default_region_code)) {
          is_valid = false;
          if (error_message) {
            *error_message = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_PHONE_INVALID_VALIDATION_MESSAGE);
          }
        }
        break;
      }

      case autofill::EMAIL_ADDRESS: {
        if (!autofill::IsValidEmailAddress(textfield->GetText())) {
          is_valid = false;
          if (error_message) {
            *error_message = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_EMAIL_INVALID_VALIDATION_MESSAGE);
          }
        }
        break;
      }

      case autofill::NAME_FULL: {
        // We have already determined that name is nonempty, which is the only
        // requirement.
        break;
      }

      default: {
        NOTREACHED();
        break;
      }
    }
  }

  return is_valid;
}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    IsValidCombobox(views::Combobox* combobox, base::string16* error_message) {
  // This UI doesn't contain any comboboxes.
  NOTREACHED();
  return true;
}

bool ContactInfoEditorViewController::ContactInfoValidationDelegate::
    ComboboxValueChanged(views::Combobox* combobox) {
  // This UI doesn't contain any comboboxes.
  NOTREACHED();
  return true;
}

}  // namespace payments
