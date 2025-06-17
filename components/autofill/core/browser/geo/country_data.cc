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

// Maps country codes to address import requirements.
// This list is comprised of countries appearing in both
// //third_party/icu/source/data/region/en.txt and
// //third_party/libaddressinput/src/cpp/src/region_data_constants.cc.
constexpr auto kCountryAddressImportRequirementsData =
    base::MakeFixedFlatMap<std::string_view, RequiredFieldsForAddressImport>(
        {{"AC", ADDRESS_REQUIRES_LINE1_CITY},
         {"AD", ADDRESS_REQUIRES_LINE1},
         {"AE", ADDRESS_REQUIRES_LINE1_STATE},
         {"AF", ADDRESS_REQUIRES_LINE1_CITY},
         {"AG", ADDRESS_REQUIRES_LINE1},
         {"AI", ADDRESS_REQUIRES_LINE1_CITY},
         {"AL", ADDRESS_REQUIRES_LINE1_CITY},
         {"AM", ADDRESS_REQUIRES_LINE1_CITY},
         {"AO", ADDRESS_REQUIRES_LINE1_CITY},
         {"AQ", ADDRESS_REQUIRES_LINE1_CITY},
         {"AR", ADDRESS_REQUIRES_LINE1_CITY},
         {"AS", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"AT", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"AU", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"AW", ADDRESS_REQUIRES_LINE1_CITY},
         {"AX", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"AZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"BA", ADDRESS_REQUIRES_LINE1_CITY},
         {"BB", ADDRESS_REQUIRES_LINE1_CITY},
         {"BD", ADDRESS_REQUIRES_LINE1_CITY},
         {"BE", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"BF", ADDRESS_REQUIRES_LINE1_CITY},
         {"BG", ADDRESS_REQUIRES_LINE1_CITY},
         {"BH", ADDRESS_REQUIRES_LINE1_CITY},
         {"BI", ADDRESS_REQUIRES_LINE1_CITY},
         {"BJ", ADDRESS_REQUIRES_LINE1_CITY},
         {"BL", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"BM", ADDRESS_REQUIRES_LINE1_CITY},
         {"BN", ADDRESS_REQUIRES_LINE1_CITY},
         {"BO", ADDRESS_REQUIRES_LINE1_CITY},
         {"BQ", ADDRESS_REQUIRES_LINE1_CITY},
         {"BR", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"BS", ADDRESS_REQUIRES_LINE1_CITY},
         {"BT", ADDRESS_REQUIRES_LINE1_CITY},
         {"BV", ADDRESS_REQUIRES_LINE1_CITY},
         {"BW", ADDRESS_REQUIRES_LINE1_CITY},
         {"BY", ADDRESS_REQUIRES_LINE1_CITY},
         {"BZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"CA", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"CC", ADDRESS_REQUIRES_LINE1_CITY},
         {"CD", ADDRESS_REQUIRES_LINE1_CITY},
         {"CF", ADDRESS_REQUIRES_LINE1_CITY},
         {"CG", ADDRESS_REQUIRES_LINE1_CITY},
         {"CH", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"CI", ADDRESS_REQUIRES_LINE1_CITY},
         {"CK", ADDRESS_REQUIRES_LINE1_CITY},
         {"CL", ADDRESS_REQUIRES_LINE1_CITY},
         {"CM", ADDRESS_REQUIRES_LINE1_CITY},
         {"CN", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"CO", ADDRESS_REQUIRES_LINE1_STATE},
         {"CR", ADDRESS_REQUIRES_LINE1_STATE},
         {"CU", ADDRESS_REQUIRES_LINE1_CITY},
         {"CV", ADDRESS_REQUIRES_LINE1_CITY},
         {"CW", ADDRESS_REQUIRES_LINE1_CITY},
         {"CX", ADDRESS_REQUIRES_LINE1_CITY},
         {"CY", ADDRESS_REQUIRES_LINE1_CITY},
         {"CZ", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"DE", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"DJ", ADDRESS_REQUIRES_LINE1_CITY},
         {"DK", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"DM", ADDRESS_REQUIRES_LINE1_CITY},
         {"DO", ADDRESS_REQUIRES_LINE1_CITY},
         {"DZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"EC", ADDRESS_REQUIRES_LINE1_CITY},
         {"EE", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"EG", ADDRESS_REQUIRES_LINE1_CITY},
         {"EH", ADDRESS_REQUIRES_LINE1_CITY},
         {"ER", ADDRESS_REQUIRES_LINE1_CITY},
         {"ES", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"ET", ADDRESS_REQUIRES_LINE1_CITY},
         {"FI", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"FJ", ADDRESS_REQUIRES_LINE1_CITY},
         {"FK", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"FM", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"FO", ADDRESS_REQUIRES_LINE1_CITY},
         {"FR", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GA", ADDRESS_REQUIRES_LINE1_CITY},
         {"GB", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GD", ADDRESS_REQUIRES_LINE1_CITY},
         {"GE", ADDRESS_REQUIRES_LINE1_CITY},
         {"GF", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GG", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GH", ADDRESS_REQUIRES_LINE1_CITY},
         {"GI", ADDRESS_REQUIRES_LINE1},
         {"GL", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GM", ADDRESS_REQUIRES_LINE1_CITY},
         {"GN", ADDRESS_REQUIRES_LINE1_CITY},
         {"GP", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GQ", ADDRESS_REQUIRES_LINE1_CITY},
         {"GR", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GS", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GT", ADDRESS_REQUIRES_LINE1_CITY},
         {"GU", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"GW", ADDRESS_REQUIRES_LINE1_CITY},
         {"GY", ADDRESS_REQUIRES_LINE1_CITY},
         {"HK", ADDRESS_REQUIRES_LINE1_STATE},
         {"HM", ADDRESS_REQUIRES_LINE1_CITY},
         {"HN", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"HR", ADDRESS_REQUIRES_LINE1_CITY},
         {"HT", ADDRESS_REQUIRES_LINE1_CITY},
         {"HU", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"ID", ADDRESS_REQUIRES_LINE1_STATE},
         {"IE", ADDRESS_REQUIRES_LINE1_CITY},
         {"IL", ADDRESS_REQUIRES_LINE1_CITY},
         {"IM", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"IN", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"IO", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"IQ", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"IR", ADDRESS_REQUIRES_LINE1_CITY},
         {"IS", ADDRESS_REQUIRES_LINE1_CITY},
         {"IT", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"JE", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"JM", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"JO", ADDRESS_REQUIRES_LINE1_CITY},
         {"JP", ADDRESS_REQUIRES_LINE1_STATE_ZIP},
         {"KE", ADDRESS_REQUIRES_LINE1_CITY},
         {"KG", ADDRESS_REQUIRES_LINE1_CITY},
         {"KH", ADDRESS_REQUIRES_LINE1_CITY},
         {"KI", ADDRESS_REQUIRES_LINE1_CITY},
         {"KM", ADDRESS_REQUIRES_LINE1_CITY},
         {"KN", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"KP", ADDRESS_REQUIRES_LINE1_CITY},
         {"KR", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"KW", ADDRESS_REQUIRES_LINE1_CITY},
         {"KY", ADDRESS_REQUIRES_LINE1_STATE},
         {"KZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"LA", ADDRESS_REQUIRES_LINE1_CITY},
         {"LB", ADDRESS_REQUIRES_LINE1_CITY},
         {"LC", ADDRESS_REQUIRES_LINE1_CITY},
         {"LI", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"LK", ADDRESS_REQUIRES_LINE1_CITY},
         {"LR", ADDRESS_REQUIRES_LINE1_CITY},
         {"LS", ADDRESS_REQUIRES_LINE1_CITY},
         {"LT", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"LU", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"LV", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"LY", ADDRESS_REQUIRES_LINE1_CITY},
         {"MA", ADDRESS_REQUIRES_LINE1_CITY},
         {"MC", ADDRESS_REQUIRES_LINE1_CITY},
         {"MD", ADDRESS_REQUIRES_LINE1_CITY},
         {"ME", ADDRESS_REQUIRES_LINE1_CITY},
         {"MF", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"MG", ADDRESS_REQUIRES_LINE1_CITY},
         {"MH", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"MK", ADDRESS_REQUIRES_LINE1_CITY},
         {"ML", ADDRESS_REQUIRES_LINE1_CITY},
         {"MM", ADDRESS_REQUIRES_LINE1_CITY},
         {"MN", ADDRESS_REQUIRES_LINE1_CITY},
         {"MO", ADDRESS_REQUIRES_LINE1},
         {"MP", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"MQ", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"MR", ADDRESS_REQUIRES_LINE1_CITY},
         {"MS", ADDRESS_REQUIRES_LINE1_CITY},
         {"MT", ADDRESS_REQUIRES_LINE1_CITY},
         {"MU", ADDRESS_REQUIRES_LINE1_CITY},
         {"MV", ADDRESS_REQUIRES_LINE1_CITY},
         {"MW", ADDRESS_REQUIRES_LINE1_CITY},
         {"MX", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"MY", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"MZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"NA", ADDRESS_REQUIRES_LINE1_CITY},
         {"NC", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"NE", ADDRESS_REQUIRES_LINE1_CITY},
         {"NF", ADDRESS_REQUIRES_LINE1_CITY},
         {"NG", ADDRESS_REQUIRES_LINE1_CITY},
         {"NI", ADDRESS_REQUIRES_LINE1_CITY},
         {"NL", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"NO", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"NP", ADDRESS_REQUIRES_LINE1_CITY},
         {"NR", ADDRESS_REQUIRES_LINE1_STATE},
         {"NU", ADDRESS_REQUIRES_LINE1_CITY},
         {"NZ", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"OM", ADDRESS_REQUIRES_LINE1_CITY},
         {"PA", ADDRESS_REQUIRES_LINE1_CITY},
         {"PE", ADDRESS_REQUIRES_LINE1_CITY},
         {"PF", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"PG", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"PH", ADDRESS_REQUIRES_LINE1_CITY},
         {"PK", ADDRESS_REQUIRES_LINE1_CITY},
         {"PL", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"PM", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"PN", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"PR", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"PS", ADDRESS_REQUIRES_LINE1_CITY},
         {"PT", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"PW", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"PY", ADDRESS_REQUIRES_LINE1_CITY},
         {"QA", ADDRESS_REQUIRES_LINE1_CITY},
         {"RE", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"RO", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"RS", ADDRESS_REQUIRES_LINE1_CITY},
         {"RU", ADDRESS_REQUIRES_LINE1_CITY},
         {"RW", ADDRESS_REQUIRES_LINE1_CITY},
         {"SA", ADDRESS_REQUIRES_LINE1_CITY},
         {"SB", ADDRESS_REQUIRES_LINE1_CITY},
         {"SC", ADDRESS_REQUIRES_LINE1_CITY},
         {"SD", ADDRESS_REQUIRES_LINE1_CITY},
         {"SE", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"SG", ADDRESS_REQUIRES_LINE1_ZIP},
         {"SH", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"SI", ADDRESS_REQUIRES_LINE1_CITY},
         {"SJ", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"SK", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"SL", ADDRESS_REQUIRES_LINE1_CITY},
         {"SM", ADDRESS_REQUIRES_LINE1_ZIP},
         {"SN", ADDRESS_REQUIRES_LINE1_CITY},
         {"SO", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"SR", ADDRESS_REQUIRES_LINE1_CITY},
         {"SS", ADDRESS_REQUIRES_LINE1_CITY},
         {"ST", ADDRESS_REQUIRES_LINE1_CITY},
         {"SV", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"SX", ADDRESS_REQUIRES_LINE1_CITY},
         {"SY", ADDRESS_REQUIRES_LINE1_CITY},
         {"SZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"TA", ADDRESS_REQUIRES_LINE1_CITY},
         {"TC", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"TD", ADDRESS_REQUIRES_LINE1_CITY},
         {"TF", ADDRESS_REQUIRES_LINE1_CITY},
         {"TG", ADDRESS_REQUIRES_LINE1_CITY},
         {"TH", ADDRESS_REQUIRES_LINE1_CITY},
         {"TJ", ADDRESS_REQUIRES_LINE1_CITY},
         {"TK", ADDRESS_REQUIRES_LINE1_CITY},
         {"TL", ADDRESS_REQUIRES_LINE1_CITY},
         {"TM", ADDRESS_REQUIRES_LINE1_CITY},
         {"TN", ADDRESS_REQUIRES_LINE1_CITY},
         {"TO", ADDRESS_REQUIRES_LINE1_CITY},
         {"TR", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"TT", ADDRESS_REQUIRES_LINE1_CITY},
         {"TV", ADDRESS_REQUIRES_LINE1_CITY},
         {"TW", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"TZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"UA", ADDRESS_REQUIRES_LINE1_CITY},
         {"UG", ADDRESS_REQUIRES_LINE1_CITY},
         {"UM", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"US", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"UY", ADDRESS_REQUIRES_LINE1_CITY},
         {"UZ", ADDRESS_REQUIRES_LINE1_CITY},
         {"VA", ADDRESS_REQUIRES_LINE1_CITY},
         {"VC", ADDRESS_REQUIRES_LINE1_CITY},
         {"VE", ADDRESS_REQUIRES_LINE1_CITY_STATE},
         {"VG", ADDRESS_REQUIRES_LINE1},
         {"VI", ADDRESS_REQUIRES_LINE1_CITY_STATE_ZIP},
         {"VN", ADDRESS_REQUIRES_LINE1_CITY},
         {"VU", ADDRESS_REQUIRES_LINE1_CITY},
         {"WF", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"WS", ADDRESS_REQUIRES_LINE1_CITY},
         {"XK", ADDRESS_REQUIRES_LINE1_CITY},
         {"YE", ADDRESS_REQUIRES_LINE1_CITY},
         {"YT", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"ZA", ADDRESS_REQUIRES_LINE1_CITY_ZIP},
         {"ZM", ADDRESS_REQUIRES_LINE1_CITY},
         {"ZW", ADDRESS_REQUIRES_LINE1_CITY}});

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
