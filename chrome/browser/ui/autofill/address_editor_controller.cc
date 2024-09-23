// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_editor_controller.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AddressEditorController::AddressEditorController(
    const AutofillProfile& profile_to_edit,
    PersonalDataManager* pdm,
    bool is_validatable)
    : profile_to_edit_(profile_to_edit),
      pdm_(*pdm),
      locale_(g_browser_process->GetApplicationLocale()),
      is_validatable_(is_validatable) {
  base::RepeatingCallback<bool(const std::string&)> filter;
  if (should_filter_out_unsupported_countries()) {
    // TODO(crbug.com/40263955): remove temporary unsupported countries
    // filtering.
    filter = base::BindRepeating(
        [](const PersonalDataManager* personal_data,
           const std::string& country) {
          return personal_data->address_data_manager()
              .IsCountryEligibleForAccountStorage(country);
        },
        &pdm_.get());
  }
  countries_.SetCountries(pdm_.get(), std::move(filter), locale_);
  std::u16string profile_country_code =
      profile_to_edit_.GetRawInfo(ADDRESS_HOME_COUNTRY);
  CHECK(!profile_country_code.empty());
  UpdateEditorFields(base::UTF16ToASCII(profile_country_code));
}

AddressEditorController::~AddressEditorController() = default;

CountryComboboxModel& AddressEditorController::GetCountryComboboxModel() {
  return countries_;
}

void AddressEditorController::UpdateEditorFields(
    const std::string& country_code) {
  editor_fields_.clear();

  std::vector<std::vector<AutofillAddressUIComponent>> components;
  GetAddressComponents(country_code, locale_,
                       /*include_literals=*/false, &components,
                       &language_code_);
  profile_to_edit_.set_language_code(language_code_);

  // Insert the Country combobox at the top.
  editor_fields_.emplace_back(
      ADDRESS_HOME_COUNTRY,
      l10n_util::GetStringUTF16(IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL),
      EditorField::LengthHint::HINT_LONG, /*is_required=*/false,
      EditorField::ControlType::COMBOBOX);

  for (const std::vector<AutofillAddressUIComponent>& line : components) {
    for (const AutofillAddressUIComponent& component : line) {
      EditorField::LengthHint length_hint =
          component.length_hint == AutofillAddressUIComponent::HINT_LONG
              ? EditorField::LengthHint::HINT_LONG
              : EditorField::LengthHint::HINT_SHORT;
      EditorField::ControlType control_type =
          component.field == ADDRESS_HOME_COUNTRY
              ? EditorField::ControlType::COMBOBOX
              : EditorField::ControlType::TEXTFIELD;

      editor_fields_.emplace_back(
          component.field, base::UTF8ToUTF16(component.name), length_hint,
          component.is_required, control_type);
    }
  }
  // Always add phone number and email at the end.
  editor_fields_.emplace_back(
      PHONE_HOME_WHOLE_NUMBER,
      l10n_util::GetStringUTF16(IDS_SETTINGS_AUTOFILL_ADDRESSES_PHONE),
      EditorField::LengthHint::HINT_SHORT, /*is_required=*/false,
      EditorField::ControlType::TEXTFIELD_NUMBER);

  editor_fields_.emplace_back(
      EMAIL_ADDRESS,
      l10n_util::GetStringUTF16(IDS_SETTINGS_AUTOFILL_ADDRESSES_EMAIL),
      EditorField::LengthHint::HINT_LONG, /*is_required=*/false,
      EditorField::ControlType::TEXTFIELD);
}

void AddressEditorController::SetProfileInfo(FieldType type,
                                             const std::u16string& value) {
  // Since the countries combobox contains the country names, not the country
  // codes, and hence we should use `SetInfo()` to make sure they get converted
  // to country codes. Also use `SetInfo()` for `NAME_FULL` so that its
  // dependent nodes (NAME_FIRST, NAME_LAST, etc) are also updated. This does
  // not need to be done for addresses because it is handled internally inside
  // `SetRawInfo()`.
  if (type == ADDRESS_HOME_COUNTRY || type == NAME_FULL) {
    profile_to_edit_.SetInfoWithVerificationStatus(
        type, value, locale_, VerificationStatus::kUserVerified);
    return;
  }

  profile_to_edit_.SetRawInfoWithVerificationStatus(
      type, value, VerificationStatus::kUserVerified);
}

std::u16string AddressEditorController::GetProfileInfo(FieldType type) {
  // TDOD(mamir): Update the implementation to format strings properly.
  return profile_to_edit_.GetInfo(type, locale_);
}

const AutofillProfile& AddressEditorController::GetAddressProfile() {
  return profile_to_edit_;
}

void AddressEditorController::SetIsValid(bool is_valid) {
  bool should_notify = is_valid_ != is_valid;
  is_valid_ = is_valid;
  if (should_notify) {
    on_is_valid_change_callbacks_.Notify(is_valid);
  }
}

base::CallbackListSubscription
AddressEditorController::AddIsValidChangedCallback(
    OnIsValidChangeCallbackList::CallbackType callback) {
  return on_is_valid_change_callbacks_.Add(std::move(callback));
}

bool AddressEditorController::IsValid(const EditorField& field,
                                      const std::u16string& value) {
  if (is_validatable_ && field.is_required &&
      base::CollapseWhitespace(value, true).empty()) {
    return false;
  }

  return true;
}

}  // namespace autofill
