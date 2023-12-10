// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/autofill_country.h"

#include <stddef.h>
#include <array>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
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
    base::MakeFixedFlatMap<ServerFieldType, RequiredFieldsForAddressImport>(
        {{ServerFieldType::ADDRESS_HOME_STATE,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_STATE},
         {ServerFieldType::ADDRESS_HOME_CITY,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_CITY},
         {ServerFieldType::ADDRESS_HOME_STREET_ADDRESS,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_LINE1},
         {ServerFieldType::ADDRESS_HOME_ZIP,
          RequiredFieldsForAddressImport::ADDRESS_REQUIRES_ZIP}});

// Autofill is experimenting with a looser set of requirements based on a newer
// version of libaddressinput. If the experiment is successful, those
// requirements will become the default ones. For now the differences are
// hardcoded using that map.
// Set of countries where the current autofill country-specific address import
// requirements differ from the updated ones.
constexpr auto kAddressRequirementExceptionCountries =
    base::MakeFixedFlatSet<base::StringPiece>(
        {"AF", "AI", "AL", "AM", "AR", "AZ", "BA", "BB", "BD", "BG", "BH", "BM",
         "BN", "BS", "BT", "CC", "CL", "CO", "CR", "CV", "CX", "CY", "DO", "DZ",
         "EC", "EH", "ET", "FO", "GE", "GN", "GT", "GW", "HM", "HR", "HT", "ID",
         "IE", "IL", "IR", "IS", "JO", "KE", "KG", "KH", "KI", "KP", "KW", "KZ",
         "LA", "LB", "LK", "LR", "LS", "MA", "MC", "MD", "ME", "MG", "MK", "MM",
         "MN", "MT", "MU", "MV", "MZ", "NA", "NE", "NF", "NG", "NI", "NP", "OM",
         "PA", "PE", "PK", "PY", "RS", "SA", "SC", "SI", "SN", "SR", "SZ", "TH",
         "TJ", "TM", "TN", "TV", "TZ", "UY", "UZ", "VA", "VC"});

// Gets the country-specific field requirements for address import.
RequiredFieldsForAddressImport GetRequiredFieldsForAddressImport(
    const std::string& country_code) {
  if (kAddressRequirementExceptionCountries.contains(country_code) &&
      base::FeatureList::IsEnabled(
          features::kAutofillUseUpdatedRequiredFieldsForAddressImport)) {
    if (country_code == "CO" || country_code == "ID" || country_code == "CR") {
      return ADDRESS_REQUIRES_LINE1_STATE;
    } else {
      return ADDRESS_REQUIRES_LINE1_CITY;
    }
  } else {
    return CountryDataMap::GetInstance()->GetRequiredFieldsForAddressImport(
        country_code);
  }
}

}  // namespace

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const absl::optional<std::string>& locale) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();

  // If the country code is an alias (e.g. "GB" for "UK") expand the country
  // code.
  country_code_ = country_data_map->HasCountryCodeAlias(country_code)
                      ? country_data_map->GetCountryCodeForAlias(country_code)
                      : country_code;

  required_fields_for_address_import_ =
      GetRequiredFieldsForAddressImport(country_code_);

  // Translate the country name by the supplied local.
  if (locale)
    name_ = l10n_util::GetDisplayNameForCountry(country_code_, *locale);
}

AutofillCountry::~AutofillCountry() = default;

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
      {{.type = ServerFieldType::ADDRESS_HOME_STATE,
        .label_id = IDS_LIBADDRESSINPUT_PROVINCE,
        .placed_after = ServerFieldType::ADDRESS_HOME_CITY,
        .separator_before_label = "\n",
        .large_sized = true}}};
  static constexpr std::array<AddressFormatExtension, 1> gb_extensions{
      {{.type = ServerFieldType::ADDRESS_HOME_STATE,
        .label_id = IDS_LIBADDRESSINPUT_COUNTY,
        .placed_after = ServerFieldType::ADDRESS_HOME_ZIP,
        .separator_before_label = "\n",
        .large_sized = true}}};
  static constexpr std::array<AddressFormatExtension, 1> mx_extensions{
      {{.type = ServerFieldType::ADDRESS_HOME_ADMIN_LEVEL2,
        .label_id = IDS_AUTOFILL_ADDRESS_EDIT_DIALOG_HISPANIC_MUNICIPIO,
        .placed_after = ServerFieldType::ADDRESS_HOME_DEPENDENT_LOCALITY,
        .separator_before_label = "\n",
        .large_sized = true}}};

  std::vector<std::pair<std::string, base::span<const AddressFormatExtension>>>
      overrides = {{"FR", fr_extensions}, {"GB", gb_extensions}};

  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForAdminLevel2)) {
    overrides.emplace_back("MX", mx_extensions);
  }

  auto extensions =
      base::MakeFlatMap<std::string, base::span<const AddressFormatExtension>>(
          std::move(overrides));

  auto it = extensions.find(country_code_);
  if (it != extensions.end())
    return it->second;
  return {};
}

bool AutofillCountry::IsAddressFieldSettingAccessible(
    ServerFieldType field_type) const {
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

bool AutofillCountry::IsAddressFieldRequired(ServerFieldType field_type) const {
  auto* mapping_it = kRequiredFieldMapping.find(field_type);
  return mapping_it != kRequiredFieldMapping.end() &&
         (required_fields_for_address_import_ & mapping_it->second);
}
}  // namespace autofill
