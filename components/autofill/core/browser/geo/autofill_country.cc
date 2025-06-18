// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/autofill_country.h"

#include <stddef.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/libaddressinput/messages.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// The maximum capacity needed to store a locale up to the country code.
constexpr size_t kLocaleCapacity =
    ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY + 1;

// Mapping of fields needed for identifying libaddressinput fields that
// considered required in Autofill.
constexpr auto kRequiredFieldMapping =
    base::MakeFixedFlatMap<FieldType, RequiredFieldsForAddressImport>(
        {{FieldType::ADDRESS_HOME_STATE,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_STATE},
         {FieldType::ADDRESS_HOME_CITY,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_CITY},
         {FieldType::ADDRESS_HOME_STREET_ADDRESS,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_LINE1},
         {FieldType::ADDRESS_HOME_ZIP,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_ZIP}});

}  // namespace

AutofillCountry::AutofillCountry(std::string_view country_code,
                                 std::optional<std::string_view> locale) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();

  // If the country code is an alias (e.g. "GB" for "UK") expand the country
  // code.
  country_code_ = country_data_map->HasCountryCodeAlias(country_code)
                      ? country_data_map->GetCountryCodeForAlias(country_code)
                      : std::string(country_code);

  required_fields_for_address_import_ =
      country_data_map->GetRequiredFieldsForAddressImport(country_code_);

  // Translate the country name by the supplied local.
  if (locale)
    name_ = l10n_util::GetDisplayNameForCountry(country_code_, *locale);
}

AutofillCountry::~AutofillCountry() = default;

// static
std::string AutofillCountry::CountryCodeForLocale(std::string_view locale) {
  // Add likely subtags to the locale. In particular, add any likely country
  // subtags -- e.g. for locales like "ru" that only include the language.
  // std::string likely_locale;
  UErrorCode error_ignored = U_ZERO_ERROR;
  std::array<char, kLocaleCapacity> likely_locale = {};
  uloc_addLikelySubtags(std::string(locale).c_str(), likely_locale.data(),
                        kLocaleCapacity, &error_ignored);

  // Extract the country code.
  std::string country_code = icu::Locale(likely_locale.data()).getCountry();

  // Default to the United States if we have no better guess.
  if (!base::Contains(CountryDataMap::GetInstance()->country_codes(),
                      country_code)) {
    return "US";
  }

  return country_code;
}

// static
AddressCountryCode AutofillCountry::GetDefaultCountryCodeForNewAddress(
    const GeoIpCountryCode& geo_ip_country_code,
    std::string_view locale) {
  // Capitalize the country code, because some APIs might not allow the usage of
  // lowercase country codes.
  return AddressCountryCode(
      base::ToUpperASCII(geo_ip_country_code.value().empty()
                             ? AutofillCountry::CountryCodeForLocale(locale)
                             : geo_ip_country_code.value()));
}

// Prints a formatted log of a |AutofillCountry| to a |LogBuffer|.
LogBuffer& operator<<(LogBuffer& buffer, const AutofillCountry& country) {
  buffer << LogMessage::kImportAddressProfileFromFormAddressRequirements;
  buffer << Tag{"div"} << Attrib{"class", "country_data"};
  buffer << Tag{"table"};
  buffer << Tr{} << "Country code:" << country.country_code();
  buffer << Tr{} << "Country name:" << country.name();
  buffer << Tr{} << "State required:" << country.requires_state();
  buffer << Tr{} << "Zip required:" << country.requires_zip();
  buffer << Tr{} << "City required:" << country.requires_city();
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  buffer << CTag{};
  return buffer;
}

base::span<const AutofillCountry::AddressFormatExtension>
AutofillCountry::address_format_extensions() const {
  // TODO(crbug.com/40216312): Extend more countries. FR and GB already have
  // overwrites, because libaddressinput already provides string literals.
  static constexpr std::array<AddressFormatExtension, 2> fr_extensions{
      {{.type = FieldType::ADDRESS_HOME_STATE,
        .label_id = IDS_LIBADDRESSINPUT_PROVINCE,
        .placed_after = FieldType::ADDRESS_HOME_CITY,
        .separator_before_label = "\n",
        .large_sized = true},
       {.type = FieldType::ADDRESS_HOME_DEPENDENT_LOCALITY,
        .label_id = IDS_AUTOFILL_ADDRESS_EDIT_DIALOG_FRENCH_LOCALITY_2,
        .placed_after = FieldType::ADDRESS_HOME_STREET_ADDRESS,
        .separator_before_label = "\n",
        .large_sized = true}}};
  static constexpr std::array<AddressFormatExtension, 1> gb_extensions{
      {{.type = FieldType::ADDRESS_HOME_STATE,
        .label_id = IDS_LIBADDRESSINPUT_COUNTY,
        .placed_after = FieldType::ADDRESS_HOME_ZIP,
        .separator_before_label = "\n",
        .large_sized = true}}};
  static constexpr std::array<AddressFormatExtension, 1> mx_extensions{
      {{.type = FieldType::ADDRESS_HOME_ADMIN_LEVEL2,
        .label_id = IDS_AUTOFILL_ADDRESS_EDIT_DIALOG_HISPANIC_MUNICIPIO,
        .placed_after = FieldType::ADDRESS_HOME_DEPENDENT_LOCALITY,
        .separator_before_label = "\n",
        .large_sized = true}}};
  static constexpr std::array<AddressFormatExtension, 1> de_extensions{
      {{.type = FieldType::ADDRESS_HOME_STATE,
        .label_id = IDS_LIBADDRESSINPUT_STATE,
        .placed_after = FieldType::ADDRESS_HOME_CITY,
        .separator_before_label = " "}}};
  static constexpr std::array<AddressFormatExtension, 1> pl_extensions{
      {{.type = FieldType::ADDRESS_HOME_STATE,
        .label_id = IDS_LIBADDRESSINPUT_STATE,
        .placed_after = FieldType::ADDRESS_HOME_CITY,
        .separator_before_label = " "}}};
  static constexpr std::array<AddressFormatExtension, 1> jp_extensions{
      {{.type = FieldType::ALTERNATIVE_FULL_NAME,
        .label_id = IDS_AUTOFILL_ADDRESS_EDIT_DIALOG_JAPANESE_ALTERNATIVE_NAME,
        .placed_after = FieldType::NAME_FULL,
        .separator_before_label = "\n",
        .large_sized = true}}};

  std::vector<std::pair<std::string, base::span<const AddressFormatExtension>>>
      overrides = {{"DE", de_extensions},
                   {"FR", fr_extensions},
                   {"GB", gb_extensions},
                   {"MX", mx_extensions},
                   {"PL", pl_extensions}};

  if (base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    overrides.emplace_back("JP", jp_extensions);
  }

  auto extensions =
      base::flat_map<std::string, base::span<const AddressFormatExtension>>(
          std::move(overrides));

  auto it = extensions.find(country_code_);
  if (it != extensions.end())
    return it->second;
  return {};
}

bool AutofillCountry::IsAddressFieldSettingAccessible(
    FieldType field_type) const {
  ::i18n::addressinput::AddressField libaddressinput_field;
  bool is_valid_field = i18n::FieldForType(field_type, &libaddressinput_field);
  // Check if `field_type` is part of libaddressinput's native address format
  // or part of the Autofill's address extensions.
  return (is_valid_field && ::i18n::addressinput::IsFieldUsed(
                                libaddressinput_field, country_code_)) ||
         base::Contains(
             address_format_extensions(), field_type,
             [](const AddressFormatExtension& rule) { return rule.type; });
}

bool AutofillCountry::IsAddressFieldRequired(FieldType field_type) const {
  auto mapping_it = kRequiredFieldMapping.find(field_type);
  return mapping_it != kRequiredFieldMapping.end() &&
         (required_fields_for_address_import_ & mapping_it->second);
}
}  // namespace autofill
