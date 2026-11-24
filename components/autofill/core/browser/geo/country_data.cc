// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_data.h"

#include <algorithm>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/memory/singleton.h"

namespace autofill {
namespace {

// Alias definitions. A request for the key is served with the `CountryData` for
// the target.
constexpr auto kCountryCodeAliases =
    base::MakeFixedFlatMap<FixedCountryCode, FixedCountryCode>({{"UK", "GB"}});

static_assert(std::ranges::all_of(kCountryCodeAliases, [](const auto& entry) {
  return kCountryAddressImportRequirementsData.contains(entry.second);
}));

}  // namespace

// static
CountryDataMap* CountryDataMap::GetInstance() {
  return base::Singleton<CountryDataMap>::get();
}

CountryDataMap::CountryDataMap() = default;

CountryDataMap::~CountryDataMap() = default;

bool CountryDataMap::HasCountryCode(std::string_view country_code) const {
  return kCountryAddressImportRequirementsData.contains(country_code);
}

RequiredFieldsForAddressImport
CountryDataMap::GetRequiredFieldsForAddressImport(
    std::string_view country_code) const {
  if (const auto* required_fields = base::FindOrNull(
          kCountryAddressImportRequirementsData, country_code)) {
    return *required_fields;
  }
  return RequiredFieldsForAddressImport::ADDRESS_REQUIREMENTS_UNKNOWN;
}

bool CountryDataMap::HasCountryCodeAlias(
    std::string_view country_code_alias) const {
  return kCountryCodeAliases.contains(country_code_alias);
}

std::string_view CountryDataMap::GetCountryCodeForAlias(
    std::string_view country_code_alias) const {
  if (const auto* country_code =
          base::FindOrNull(kCountryCodeAliases, country_code_alias)) {
    return std::string_view(*country_code);
  }
  return {};
}

}  // namespace autofill
