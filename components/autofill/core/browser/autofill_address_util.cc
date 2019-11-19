// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_address_util.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::AutofillCountry;
using autofill::ServerFieldType;
using i18n::addressinput::AddressUiComponent;

namespace autofill {

// Dictionary keys for address components info.
const char kFieldTypeKey[] = "field";
const char kFieldLengthKey[] = "isLongField";
const char kFieldNameKey[] = "fieldName";

// Field names for the address components.
const char kFullNameField[] = "FULL_NAME";
const char kCompanyNameField[] = "COMPANY_NAME";
const char kAddressLineField[] = "ADDRESS_LINES";
const char kDependentLocalityField[] = "ADDRESS_LEVEL_3";
const char kCityField[] = "ADDRESS_LEVEL_2";
const char kStateField[] = "ADDRESS_LEVEL_1";
const char kPostalCodeField[] = "POSTAL_CODE";
const char kSortingCodeField[] = "SORTING_CODE";
const char kCountryField[] = "COUNTY_CODE";

// Address field length values.
const bool kShortField = false;
const bool kLongField = true;

ServerFieldType GetFieldTypeFromString(const std::string& type) {
  if (type == kFullNameField)
    return NAME_FULL;
  if (type == kCompanyNameField)
    return COMPANY_NAME;
  if (type == kAddressLineField)
    return ADDRESS_HOME_STREET_ADDRESS;
  if (type == kDependentLocalityField)
    return ADDRESS_HOME_DEPENDENT_LOCALITY;
  if (type == kCityField)
    return ADDRESS_HOME_CITY;
  if (type == kStateField)
    return ADDRESS_HOME_STATE;
  if (type == kPostalCodeField)
    return ADDRESS_HOME_ZIP;
  if (type == kSortingCodeField)
    return ADDRESS_HOME_SORTING_CODE;
  if (type == kCountryField)
    return ADDRESS_HOME_COUNTRY;
  NOTREACHED();
  return UNKNOWN_TYPE;
}

// Fills |components| with the address UI components that should be used to
// input an address for |country_code| when UI BCP 47 language code is
// |ui_language_code|. If |components_language_code| is not NULL, then sets it
// to the BCP 47 language code that should be used to format the address for
// display.
void GetAddressComponents(const std::string& country_code,
                          const std::string& ui_language_code,
                          base::ListValue* address_components,
                          std::string* components_language_code) {
  DCHECK(address_components);

  ::i18n::addressinput::Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);
  std::string not_used;
  std::vector<AddressUiComponent> components =
      ::i18n::addressinput::BuildComponents(
          country_code, localization, ui_language_code,
          components_language_code ? components_language_code : &not_used);
  if (components.empty()) {
    static const char kDefaultCountryCode[] = "US";
    components = ::i18n::addressinput::BuildComponents(
        kDefaultCountryCode, localization, ui_language_code,
        components_language_code ? components_language_code : &not_used);
  }
  DCHECK(!components.empty());

  base::ListValue* line = nullptr;
  for (size_t i = 0; i < components.size(); ++i) {
    if (components[i].field == ::i18n::addressinput::ORGANIZATION &&
        !base::FeatureList::IsEnabled(features::kAutofillEnableCompanyName)) {
      continue;
    }
    if (i == 0 ||
        components[i - 1].length_hint == AddressUiComponent::HINT_LONG ||
        components[i].length_hint == AddressUiComponent::HINT_LONG) {
      line = new base::ListValue;
      address_components->Append(std::unique_ptr<base::ListValue>(line));
      // |line| is invalidated at this point, so it needs to be reset.
      address_components->GetList(address_components->GetSize() - 1, &line);
    }

    auto component = std::make_unique<base::DictionaryValue>();
    component->SetString(kFieldNameKey, components[i].name);

    switch (components[i].field) {
      case ::i18n::addressinput::COUNTRY:
        component->SetString(kFieldTypeKey, kCountryField);
        break;
      case ::i18n::addressinput::ADMIN_AREA:
        component->SetString(kFieldTypeKey, kStateField);
        break;
      case ::i18n::addressinput::LOCALITY:
        component->SetString(kFieldTypeKey, kCityField);
        break;
      case ::i18n::addressinput::DEPENDENT_LOCALITY:
        component->SetString(kFieldTypeKey, kDependentLocalityField);
        break;
      case ::i18n::addressinput::SORTING_CODE:
        component->SetString(kFieldTypeKey, kSortingCodeField);
        break;
      case ::i18n::addressinput::POSTAL_CODE:
        component->SetString(kFieldTypeKey, kPostalCodeField);
        break;
      case ::i18n::addressinput::STREET_ADDRESS:
        component->SetString(kFieldTypeKey, kAddressLineField);
        break;
      case ::i18n::addressinput::ORGANIZATION:
        component->SetString(kFieldTypeKey, kCompanyNameField);
        break;
      case ::i18n::addressinput::RECIPIENT:
        component->SetString(kFieldTypeKey, kFullNameField);
        break;
    }

    switch (components[i].length_hint) {
      case AddressUiComponent::HINT_LONG:
        component->SetBoolean(kFieldLengthKey, kLongField);
        break;
      case AddressUiComponent::HINT_SHORT:
        component->SetBoolean(kFieldLengthKey, kShortField);
        break;
    }

    line->Append(std::move(component));
  }
}

}  // namespace autofill
