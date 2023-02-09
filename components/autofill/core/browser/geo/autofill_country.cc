// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/autofill_country.h"

#include <stddef.h>
#include <array>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/libaddressinput/messages.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"
#include "ui/base/l10n/l10n_util.h"

using ::i18n::addressinput::AddressField;

namespace autofill {
namespace {

// The maximum capacity needed to store a locale up to the country code.
constexpr size_t kLocaleCapacity =
    ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY + 1;

// Mapping of fields needed for identifying libaddressinput fields that
// considered required in Autofill.
constexpr auto kRequiredFieldMapping =
    base::MakeFixedFlatMap<::i18n::addressinput::AddressField,
                           RequiredFieldsForAddressImport>(
        {{::i18n::addressinput::AddressField::ADMIN_AREA,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_STATE},
         {::i18n::addressinput::AddressField::LOCALITY,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_CITY},
         {::i18n::addressinput::AddressField::STREET_ADDRESS,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_LINE1},
         {::i18n::addressinput::AddressField::POSTAL_CODE,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_ZIP}});
}  // namespace

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const absl::optional<std::string>& locale) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();

  // If the country code is an alias (e.g. "GB" for "UK") expand the country
  // code.
  country_code_ = country_data_map->HasCountryCodeAlias(country_code)
                      ? country_data_map->GetCountryCodeForAlias(country_code)
                      : country_code;

  // Acquire the country address data.
  required_fields_for_address_import_ =
      country_data_map->GetRequiredFieldsForAddressImport(country_code_);

  // Translate the country name by the supplied local.
  if (locale)
    name_ = l10n_util::GetDisplayNameForCountry(country_code_, *locale);
}

AutofillCountry::~AutofillCountry() {}

// static
const std::string AutofillCountry::CountryCodeForLocale(
    const std::string& locale) {
  // Add likely subtags to the locale. In particular, add any likely country
  // subtags -- e.g. for locales like "ru" that only include the language.
  std::string likely_locale;
  UErrorCode error_ignored = U_ZERO_ERROR;
  uloc_addLikelySubtags(locale.c_str(),
                        base::WriteInto(&likely_locale, kLocaleCapacity),
                        kLocaleCapacity, &error_ignored);

  // Extract the country code.
  std::string country_code = icu::Locale(likely_locale.c_str()).getCountry();

  // Default to the United States if we have no better guess.
  if (!base::Contains(CountryDataMap::GetInstance()->country_codes(),
                      country_code)) {
    return "US";
  }

  return country_code;
}

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const std::u16string& name,
                                 const std::u16string& postal_code_label,
                                 const std::u16string& state_label)
    : country_code_(country_code), name_(name) {}

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
  // TODO(crbug.com/1300548): Extend more countries. FR and GB already have
  // overwrites, because libaddressinput already provides string literals.
  static constexpr std::array<AddressFormatExtension, 1> fr_extensions{
      {{.type = AddressField::ADMIN_AREA,
        .label_id = IDS_LIBADDRESSINPUT_PROVINCE,
        .placed_after = AddressField::LOCALITY,
        .separator_before_label = "\n",
        .large_sized = true}}};
  static constexpr std::array<AddressFormatExtension, 1> gb_extensions{
      {{.type = AddressField::ADMIN_AREA,
        .label_id = IDS_LIBADDRESSINPUT_COUNTY,
        .placed_after = AddressField::POSTAL_CODE,
        .separator_before_label = "\n",
        .large_sized = true}}};

  static constexpr auto extensions =
      base::MakeFixedFlatMap<base::StringPiece,
                             base::span<const AddressFormatExtension>>({
          {"FR", fr_extensions},
          {"GB", gb_extensions},
      });

  auto* it = extensions.find(country_code_);
  if (it != extensions.end())
    return it->second;
  return {};
}

bool AutofillCountry::IsAddressFieldSettingAccessible(
    AddressField address_field) const {
  // Check if `address_field` is part of libaddressinputs native address format
  // or part of the Autofill's address extensions.
  return ::i18n::addressinput::IsFieldUsed(address_field, country_code_) ||
         base::Contains(
             address_format_extensions(), address_field,
             [](const AddressFormatExtension& rule) { return rule.type; });
}

bool AutofillCountry::IsAddressFieldRequired(AddressField address_field) const {
  auto* mapping_it = kRequiredFieldMapping.find(address_field);
  return mapping_it != kRequiredFieldMapping.end() &&
         (required_fields_for_address_import_ & mapping_it->second);
}
}  // namespace autofill
