// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_address_util.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
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

ServerFieldType AddressFieldToServerFieldType(
    i18n::addressinput::AddressField address_field) {
  switch (address_field) {
    case ::i18n::addressinput::COUNTRY:
      return ADDRESS_HOME_COUNTRY;
    case ::i18n::addressinput::ADMIN_AREA:
      return ADDRESS_HOME_STATE;
    case ::i18n::addressinput::LOCALITY:
      return ADDRESS_HOME_CITY;
    case ::i18n::addressinput::DEPENDENT_LOCALITY:
      return ADDRESS_HOME_DEPENDENT_LOCALITY;
    case ::i18n::addressinput::SORTING_CODE:
      return ADDRESS_HOME_SORTING_CODE;
    case ::i18n::addressinput::POSTAL_CODE:
      return ADDRESS_HOME_ZIP;
    case ::i18n::addressinput::STREET_ADDRESS:
      return ADDRESS_HOME_STREET_ADDRESS;
    case ::i18n::addressinput::ORGANIZATION:
      return COMPANY_NAME;
    case ::i18n::addressinput::RECIPIENT:
      return NAME_FULL;
  }
}

// Fills |components| with the address UI components that should be used to
// input an address for |country_code| when UI BCP 47 language code is
// |ui_language_code|. If |components_language_code| is not NULL, then sets it
// to the BCP 47 language code that should be used to format the address for
// display.
void GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    std::vector<std::vector<AddressUiComponent>>* address_components,
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
  std::vector<AddressUiComponent>* line_components = nullptr;
  for (size_t i = 0; i < components.size(); ++i) {
    if (i == 0 ||
        components[i - 1].length_hint == AddressUiComponent::HINT_LONG ||
        components[i].length_hint == AddressUiComponent::HINT_LONG) {
      address_components->push_back(std::vector<AddressUiComponent>());
      line_components = &address_components->back();
    }
    line_components->push_back(components[i]);
  }
}

}  // namespace autofill
