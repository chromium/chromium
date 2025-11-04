// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_data.h"

#include <array>
#include <utility>

#include "base/containers/extend.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace autofill {
namespace {

// Alias definitions. A request for the key is served with the `CountryData` for
// the target.
constexpr auto kCountryCodeAliases =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({{"UK", "GB"}});

// GetCountryCodes and GetCountryData compute the data for CountryDataMap
// based on `kCountryAddressImportRequirementsData`.
std::vector<std::string> GetCountryCodes() {
  return base::ToVector(
      kCountryAddressImportRequirementsData,
      [](const auto& static_data) { return std::string(static_data.first); });
}

base::flat_map<std::string, RequiredFieldsForAddressImport>
GetCountryDataMap() {
  // Collect other countries that ICU knows about but for which we have no
  // manually specified requirements.
  std::vector<std::pair<std::string, RequiredFieldsForAddressImport>>
      other_countries;
  // SAFETY: `icu::Locale::getISOCountries` returns a C-style array whose last
  // entry is a nullptr.
  for (const char* const* country_pointer = icu::Locale::getISOCountries();
       *country_pointer; UNSAFE_BUFFERS(++country_pointer)) {
    if (!kCountryAddressImportRequirementsData.contains(*country_pointer)) {
      other_countries.emplace_back(
          *country_pointer,
          RequiredFieldsForAddressImport::ADDRESS_REQUIREMENTS_UNKNOWN);
    }
  }

  // Combine the other countries with those with explicit data.
  other_countries.insert(other_countries.end(),
                         kCountryAddressImportRequirementsData.begin(),
                         kCountryAddressImportRequirementsData.end());
  return base::MakeFlatMap<std::string, RequiredFieldsForAddressImport>(
      std::move(other_countries));
}

}  // namespace

// static
CountryDataMap* CountryDataMap::GetInstance() {
  return base::Singleton<CountryDataMap>::get();
}

CountryDataMap::CountryDataMap()
    : required_fields_for_address_import_map_(GetCountryDataMap()),
      country_codes_(GetCountryCodes()) {}

CountryDataMap::~CountryDataMap() = default;

bool CountryDataMap::HasRequiredFieldsForAddressImport(
    std::string_view country_code) const {
  return required_fields_for_address_import_map_.contains(country_code);
}

RequiredFieldsForAddressImport
CountryDataMap::GetRequiredFieldsForAddressImport(
    std::string_view country_code) const {
  auto lookup = required_fields_for_address_import_map_.find(country_code);
  if (lookup != required_fields_for_address_import_map_.end())
    return lookup->second;
  // If there is no entry for country_code return the entry for the US.
  return required_fields_for_address_import_map_.find("US")->second;
}

bool CountryDataMap::HasCountryCodeAlias(
    std::string_view country_code_alias) const {
  return kCountryCodeAliases.contains(country_code_alias);
}

std::string_view CountryDataMap::GetCountryCodeForAlias(
    std::string_view country_code_alias) const {
  auto lookup = kCountryCodeAliases.find(country_code_alias);
  if (lookup != kCountryCodeAliases.end()) {
    DCHECK(HasRequiredFieldsForAddressImport(lookup->second));
    return lookup->second;
  }
  return {};
}

}  // namespace autofill
