// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/country_combobox_model.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/models/combobox_model_observer.h"

namespace autofill {

CountryComboboxModel::CountryComboboxModel() = default;

CountryComboboxModel::~CountryComboboxModel() = default;

void CountryComboboxModel::SetCountries(
    const PersonalDataManager& manager,
    const base::RepeatingCallback<bool(const std::string&)>& filter,
    const std::string& app_locale) {
  countries_.clear();

  // Insert the default country at the top as well as in the ordered list.
  std::string default_country_code = manager.address_data_manager()
                                         .GetDefaultCountryCodeForNewAddress()
                                         .value();
  DCHECK(!default_country_code.empty());

  if (filter.is_null() || filter.Run(default_country_code)) {
    countries_.push_back(
        std::make_unique<AutofillCountry>(default_country_code, app_locale));
#if !BUILDFLAG(IS_ANDROID)
    // The separator item. On Android, there are separators after all items, so
    // this is unnecessary.
    countries_.push_back(nullptr);
#endif
  }

  // The sorted list of country codes.
  const std::vector<std::string>* available_countries =
      &CountryDataMap::GetInstance()->country_codes();

  // Filter out the countries that do not have rules for address input and
  // validation.
  const std::vector<std::string>& addressinput_countries =
      ::i18n::addressinput::GetRegionCodes();
  std::vector<std::string> filtered_countries;
  filtered_countries.reserve(available_countries->size());
  std::set_intersection(
      available_countries->begin(), available_countries->end(),
      addressinput_countries.begin(), addressinput_countries.end(),
      std::back_inserter(filtered_countries));
  available_countries = &filtered_countries;

  CountryVector sorted_countries;
  for (const auto& country_code : *available_countries) {
    if (filter.is_null() || filter.Run(country_code))
      sorted_countries.push_back(
          std::make_unique<AutofillCountry>(country_code, app_locale));
  }

  l10n_util::SortStringsUsingMethod(app_locale, &sorted_countries,
                                    &AutofillCountry::name);
  std::move(sorted_countries.begin(), sorted_countries.end(),
            std::back_inserter(countries_));
}

size_t CountryComboboxModel::GetItemCount() const {
  return countries_.size();
}

std::u16string CountryComboboxModel::GetItemAt(size_t index) const {
  AutofillCountry* country = countries_[index].get();
  if (country)
    return countries_[index]->name();

  // The separator item. Implemented for platforms that don't yet support
  // IsItemSeparatorAt().
  return u"---";
}

bool CountryComboboxModel::IsItemSeparatorAt(size_t index) const {
  return !countries_[index];
}

std::string CountryComboboxModel::GetDefaultCountryCode() const {
  return countries_[GetDefaultIndex().value()]->country_code();
}

}  // namespace autofill
