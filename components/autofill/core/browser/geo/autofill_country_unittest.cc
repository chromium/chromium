// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#if defined(ANDROID)
#include "base/android/build_info.h"
#endif

using base::ASCIIToUTF16;

namespace autofill {

// Test the constructor and accessors
TEST(AutofillCountryTest, AutofillCountry) {
  AutofillCountry united_states_en("US", "en_US");
  EXPECT_EQ("US", united_states_en.country_code());
  EXPECT_EQ(ASCIIToUTF16("United States"), united_states_en.name());
  EXPECT_EQ(ASCIIToUTF16("ZIP code"), united_states_en.postal_code_label());
  EXPECT_EQ(ASCIIToUTF16("State"), united_states_en.state_label());

  AutofillCountry united_states_es("US", "es");
  EXPECT_EQ("US", united_states_es.country_code());
  EXPECT_EQ(ASCIIToUTF16("Estados Unidos"), united_states_es.name());

  AutofillCountry canada_en("CA", "en_US");
  EXPECT_EQ("CA", canada_en.country_code());
  EXPECT_EQ(ASCIIToUTF16("Canada"), canada_en.name());
  EXPECT_EQ(ASCIIToUTF16("Postal code"), canada_en.postal_code_label());
  EXPECT_EQ(ASCIIToUTF16("Province"), canada_en.state_label());

  AutofillCountry canada_hu("CA", "hu");
  EXPECT_EQ("CA", canada_hu.country_code());
  EXPECT_EQ(ASCIIToUTF16("Kanada"), canada_hu.name());
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
    if (base::Contains(expected_failures, country_code))
      continue;
    SCOPED_TRACE("Country code '" + country_code + "' should have a name.");
    EXPECT_NE(ASCIIToUTF16(country_code),
              AutofillCountry(country_code, "en").name());
  }
}

}  // namespace autofill
