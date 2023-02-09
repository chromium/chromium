// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_editor_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "content/public/browser/web_contents.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"

AddressEditorController::AddressEditorController(
    const autofill::AutofillProfile& profile_to_edit,
    content::WebContents* web_contents)
    : profile_to_edit_(profile_to_edit),
      locale_(g_browser_process->GetApplicationLocale()) {
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  pdm_ = autofill::PersonalDataManagerFactory::GetForProfile(
      profile->GetOriginalProfile());

  UpdateCountries(/*model=*/nullptr);
  UpdateEditorFields();
}

AddressEditorController::~AddressEditorController() = default;

size_t AddressEditorController::GetCountriesSize() {
  return countries_.size();
}

std::unique_ptr<ui::ComboboxModel>
AddressEditorController::GetCountryComboboxModel() {
  auto model = std::make_unique<autofill::CountryComboboxModel>();
  model->SetCountries(*pdm_, /*filter=*/base::NullCallback(), locale_);
  if (model->countries().size() != countries_.size())
    UpdateCountries(model.get());
  return model;
}

void AddressEditorController::UpdateEditorFields() {
  editor_fields_.clear();
  std::string chosen_country_code;
  if (chosen_country_index_ < countries_.size())
    chosen_country_code = countries_[chosen_country_index_].first;

  std::vector<std::vector<autofill::ExtendedAddressUiComponent>> components;
  autofill::GetAddressComponents(chosen_country_code, locale_,
                                 /*include_literals=*/false, &components,
                                 &language_code_);
  profile_to_edit_.set_language_code(language_code_);

  // Insert the Country combobox at the top.
  editor_fields_.emplace_back(
      autofill::ADDRESS_HOME_COUNTRY,
      l10n_util::GetStringUTF16(IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL),
      EditorField::LengthHint::HINT_LONG, EditorField::ControlType::COMBOBOX);

  for (const std::vector<autofill::ExtendedAddressUiComponent>& line :
       components) {
    for (const autofill::ExtendedAddressUiComponent& component : line) {
      EditorField::LengthHint length_hint =
          component.length_hint ==
                  i18n::addressinput::AddressUiComponent::HINT_LONG
              ? EditorField::LengthHint::HINT_LONG
              : EditorField::LengthHint::HINT_SHORT;
      autofill::ServerFieldType server_field_type =
          autofill::i18n::TypeForField(component.field);
      EditorField::ControlType control_type =
          server_field_type == autofill::ADDRESS_HOME_COUNTRY
              ? EditorField::ControlType::COMBOBOX
              : EditorField::ControlType::TEXTFIELD;

      editor_fields_.emplace_back(server_field_type,
                                  base::UTF8ToUTF16(component.name),
                                  length_hint, control_type);
    }
  }
  // Always add phone number and email at the end.
  editor_fields_.emplace_back(
      autofill::PHONE_HOME_WHOLE_NUMBER,
      l10n_util::GetStringUTF16(IDS_SETTINGS_AUTOFILL_ADDRESSES_PHONE),
      EditorField::LengthHint::HINT_SHORT,
      EditorField::ControlType::TEXTFIELD_NUMBER);

  editor_fields_.emplace_back(
      autofill::EMAIL_ADDRESS,
      l10n_util::GetStringUTF16(IDS_SETTINGS_AUTOFILL_ADDRESSES_EMAIL),
      EditorField::LengthHint::HINT_LONG, EditorField::ControlType::TEXTFIELD);
}

void AddressEditorController::SetProfileInfo(autofill::ServerFieldType type,
                                             const std::u16string& value) {
  // Since the countries combobox contains the country names, not the country
  // codes, and hence we should use SetInfo() to make sure they get converted to
  // country codes.
  if (type == autofill::ADDRESS_HOME_COUNTRY) {
    profile_to_edit_.SetInfoWithVerificationStatus(
        type, value, locale_, autofill::VerificationStatus::kUserVerified);
    return;
  }

  profile_to_edit_.SetRawInfoWithVerificationStatus(
      type, value, autofill::VerificationStatus::kUserVerified);
}

std::u16string AddressEditorController::GetProfileInfo(
    autofill::ServerFieldType type) {
  // TDOD(mamir): Update the implementation to format strings properly.
  return profile_to_edit_.GetInfo(type, locale_);
}

void AddressEditorController::UpdateCountries(
    autofill::CountryComboboxModel* model) {
  autofill::CountryComboboxModel local_model;
  if (!model) {
    local_model.SetCountries(*pdm_, /*filter=*/base::NullCallback(), locale_);
    model = &local_model;
  }

  for (const std::unique_ptr<autofill::AutofillCountry>& country :
       model->countries()) {
    if (country) {
      countries_.emplace_back(country->country_code(), country->name());
    } else {
      // Separator, kept to make sure the size of the vector stays the same.
      countries_.emplace_back("", u"");
    }
  }

  std::u16string chosen_country(GetProfileInfo(autofill::ADDRESS_HOME_COUNTRY));
  bool found = false;
  for (size_t i = 0; i < countries_.size(); ++i) {
    if (chosen_country == countries_[i].second) {
      found = true;
      chosen_country_index_ = i;
      break;
    }
  }
  // Make sure the the country was actually found in |countries_| and was not
  // empty, otherwise set |chosen_country_index_| to index 0, which is the
  // default country based on the locale.
  if (!found || chosen_country.empty()) {
    // But only if there is at least one country.
    if (!countries_.empty()) {
      chosen_country_index_ = 0;
      SetProfileInfo(autofill::ADDRESS_HOME_COUNTRY,
                     countries_[chosen_country_index_].second);
    } else {
      chosen_country_index_ = kInvalidCountryIndex;
    }
  }
}

const autofill::AutofillProfile& AddressEditorController::GetAddressProfile() {
  return profile_to_edit_;
}
