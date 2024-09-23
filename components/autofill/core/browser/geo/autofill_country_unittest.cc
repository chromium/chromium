// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/autofill_country.h"

#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_metadata.h"
#if defined(ANDROID)
#include "base/android/build_info.h"
#endif

using autofill::CountryDataMap;
using base::ASCIIToUTF16;
using ::i18n::addressinput::AddressField;

namespace autofill {

// Test the constructor and accessors
TEST(AutofillCountryTest, AutofillCountry) {
  AutofillCountry united_states_en("US", "en_US");
  EXPECT_EQ("US", united_states_en.country_code());
  EXPECT_EQ(u"United States", united_states_en.name());

  AutofillCountry united_states_es("US", "es");
  EXPECT_EQ("US", united_states_es.country_code());
  EXPECT_EQ(u"Estados Unidos", united_states_es.name());

  AutofillCountry great_britain_uk_alias("UK", "en_GB");
  EXPECT_EQ("GB", great_britain_uk_alias.country_code());
  EXPECT_EQ("GB", great_britain_uk_alias.country_code());
  EXPECT_EQ(u"United Kingdom", great_britain_uk_alias.name());

  AutofillCountry canada_en("CA", "en_US");
  EXPECT_EQ("CA", canada_en.country_code());
  EXPECT_EQ(u"Canada", canada_en.name());

  AutofillCountry canada_hu("CA", "hu");
  EXPECT_EQ("CA", canada_hu.country_code());
  EXPECT_EQ(u"Kanada", canada_hu.name());

  // Unrecognizable country codes remain that way.
  AutofillCountry unknown("Unknown", "en_US");
  EXPECT_EQ("Unknown", unknown.country_code());

  // If no locale is provided, no `name()` is returned.
  AutofillCountry empty_locale("AT");
  EXPECT_EQ("AT", empty_locale.country_code());
  EXPECT_TRUE(empty_locale.name().empty());
}

// Test locale to country code mapping.
TEST(AutofillCountryTest, CountryCodeForLocale) {
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("en_US"));
  EXPECT_EQ("CA", AutofillCountry::CountryCodeForLocale("fr_CA"));
  EXPECT_EQ("FR", AutofillCountry::CountryCodeForLocale("fr"));
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("Unknown"));
  // "es-419" isn't associated with a country. See base/l10n/l10n_util.cc
  // for details about this locale. Default to US.
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("es-419"));
}

// Test the address requirement methods for the US.
TEST(AutofillCountryTest, UsaAddressRequirements) {
  // The US requires a zip, state, city and line1 entry.
  AutofillCountry country("US", "en_US");

  EXPECT_FALSE(country.requires_zip_or_state());
  EXPECT_TRUE(country.requires_zip());
  EXPECT_TRUE(country.requires_state());
  EXPECT_TRUE(country.requires_city());
  EXPECT_TRUE(country.requires_line1());

  // The same expectations via FieldType.
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_ZIP));
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_STATE));
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_CITY));
  EXPECT_TRUE(
      country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_STREET_ADDRESS));
}

// Test that unknown country codes have US requirements.
TEST(AutofillCountryTest, UnknownAddressRequirements) {
  AutofillCountry us_autofill_country("US", "en_US");
  AutofillCountry unknown_autofill_country("Unknown", "en_US");

  EXPECT_EQ(us_autofill_country.requires_zip_or_state(),
            unknown_autofill_country.requires_zip_or_state());
  EXPECT_EQ(us_autofill_country.requires_zip(),
            unknown_autofill_country.requires_zip());
  EXPECT_EQ(us_autofill_country.requires_state(),
            unknown_autofill_country.requires_state());
  EXPECT_EQ(us_autofill_country.requires_city(),
            unknown_autofill_country.requires_city());
  EXPECT_EQ(us_autofill_country.requires_line1(),
            unknown_autofill_country.requires_line1());
}

// Test the address requirement method for Brazil.
TEST(AutofillCountryTest, BrAddressRequirements) {
  // Brazil only requires a zip entry.
  AutofillCountry country("BR", "en_US");

  EXPECT_FALSE(country.requires_zip_or_state());
  EXPECT_TRUE(country.requires_zip());
  EXPECT_TRUE(country.requires_state());
  EXPECT_TRUE(country.requires_city());
  EXPECT_TRUE(country.requires_line1());

  // The same expectations via FieldType.
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_ZIP));
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_STATE));
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_CITY));
  EXPECT_TRUE(
      country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_STREET_ADDRESS));
}

// Test the address requirement method for Turkey.
TEST(AutofillCountryTest, TrAddressRequirements) {
  // Brazil only requires a zip entry.
  AutofillCountry country("TR", "en_US");

  // Although ZIP codes are existing in Turkey, they are commonly used.
  EXPECT_FALSE(country.requires_zip());
  // In Turkey, a district is the largest level of the address hierarchy and
  // mapped to the Autofill state.
  EXPECT_TRUE(country.requires_state());
  // And the province as the second largest level is mapped to city.
  EXPECT_TRUE(country.requires_city());
  EXPECT_TRUE(country.requires_line1());

  // The same expectations via FieldType.
  EXPECT_FALSE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_ZIP));
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_STATE));
  EXPECT_TRUE(country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_CITY));
  EXPECT_TRUE(
      country.IsAddressFieldRequired(FieldType::ADDRESS_HOME_STREET_ADDRESS));
}

// Test mapping all country codes to country names.
TEST(AutofillCountryTest, AllCountryCodesHaveCountryName) {
  std::set<std::string> expected_failures;
#if defined(ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_KITKAT) {
    expected_failures.insert("BQ");
    expected_failures.insert("SS");
    expected_failures.insert("XK");
  }
#endif
  const std::vector<std::string>& country_codes =
      CountryDataMap::GetInstance()->country_codes();
  for (const std::string& country_code : country_codes) {
    if (base::Contains(expected_failures, country_code)) {
      continue;
    }
    SCOPED_TRACE("Country code '" + country_code + "' should have a name.");
    EXPECT_NE(ASCIIToUTF16(country_code),
              AutofillCountry(country_code, "en").name());
  }
}

// Test alias mappings for falsely existing country codes.
TEST(AutofillCountryTest, AliasMappingsForCountryData) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();

  // There should be country data for the "GB".
  EXPECT_TRUE(country_data_map->HasRequiredFieldsForAddressImport("GB"));

  // Check the correctness of the alias definitions.
  EXPECT_TRUE(country_data_map->HasCountryCodeAlias("UK"));
  EXPECT_FALSE(country_data_map->HasCountryCodeAlias("does_not_exist"));

  // Query not existing mapping.
  auto expected_country_code = std::string();
  auto actual_country_code =
      country_data_map->GetCountryCodeForAlias("does_not_exist");
  EXPECT_EQ(expected_country_code, actual_country_code);

  // UK should map the GB.
  expected_country_code = "GB";
  actual_country_code = country_data_map->GetCountryCodeForAlias("UK");
  EXPECT_EQ(expected_country_code, actual_country_code);
}

// Verifies that all address format extensions correspond to types that are
// not part of libaddressinputs expected types, but that they are placed
// after a field that is present in libaddressinput.
TEST(AutofillCountryTest, VerifyAddressFormatExtensions) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();
  for (const std::string& country_code : country_data_map->country_codes()) {
    AutofillCountry country(country_code);
    for (const AutofillCountry::AddressFormatExtension& rule :
         country.address_format_extensions()) {
      // The separator should not be empty.
      EXPECT_FALSE(rule.separator_before_label.empty());
      // `rule.type` is not part of `country_code`'s address format, but
      // `rule.placed_after` is.
      ::i18n::addressinput::AddressField libaddressinput_field;
      bool is_valid_field =
          i18n::FieldForType(rule.type, &libaddressinput_field);
      EXPECT_TRUE(!is_valid_field || !::i18n::addressinput::IsFieldUsed(
                                         libaddressinput_field, country_code));

      ::i18n::addressinput::AddressField libaddressinput_place_after;
      ASSERT_TRUE(
          i18n::FieldForType(rule.placed_after, &libaddressinput_place_after));
      EXPECT_TRUE(::i18n::addressinput::IsFieldUsed(libaddressinput_place_after,
                                                    country_code));
      // `IsAddressFieldSettingAccessible` considers `rule.type`
      // setting-accessible.
      EXPECT_TRUE(country.IsAddressFieldSettingAccessible(rule.type));
    }
  }
}

// Test the address requirement method for Poland.
TEST(AutofillCountryTest, PLAddressRequirements) {
  AutofillCountry country("PL", "pl_PL");
  base::test::ScopedFeatureList enabled{features::kAutofillUsePLAddressModel};

  EXPECT_FALSE(country.requires_state());
  EXPECT_TRUE(
      country.IsAddressFieldSettingAccessible(FieldType::ADDRESS_HOME_STATE));
}

}  // namespace autofill
