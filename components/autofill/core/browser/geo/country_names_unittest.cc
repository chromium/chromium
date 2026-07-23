// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {

class TestCountryNames : public CountryNames {
 public:
  explicit TestCountryNames(const std::string& locale_name)
      : CountryNames(locale_name) {}

  ~TestCountryNames() = default;
};

// Test mapping of localized country names to country codes.
TEST(CountryNamesTest, GetCountryCode_BasicMapping) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ(en_us_names.GetCountryCode(u"United States"), "US");
  EXPECT_EQ(en_us_names.GetCountryCode(u"Canada"), "CA");
  EXPECT_EQ(en_us_names.GetCountryCode(u"Czech Republic"), "CZ");
}

TEST(CountryNamesTest, GetCountryCode_CaseInsensitiveMapping) {
  EXPECT_EQ(TestCountryNames("en_US").GetCountryCode(u"united states"), "US");
}

TEST(CountryNamesTest, GetCountryCode_CodesMapToThemselves) {
  TestCountryNames en_us_names("en_US");
  TestCountryNames fr_ca_names("fr_CA");
  EXPECT_EQ(en_us_names.GetCountryCode(u"US"), "US");
  EXPECT_EQ(en_us_names.GetCountryCode(u"hu"), "HU");
  EXPECT_EQ(fr_ca_names.GetCountryCode(u"CA"), "CA");
  EXPECT_EQ(fr_ca_names.GetCountryCode(u"mx"), "MX");
}

TEST(CountryNamesTest, GetCountryCode_BasicSynonyms) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ(en_us_names.GetCountryCode(u"United States of America"), "US");
  EXPECT_EQ(en_us_names.GetCountryCode(u"USA"), "US");
}

TEST(CountryNamesTest, GetCountryCode_OtherLocales) {
  EXPECT_EQ(TestCountryNames("es").GetCountryCode(u"Estados Unidos"), "US");
  EXPECT_EQ(TestCountryNames("it").GetCountryCode(u"Italia"), "IT");
  EXPECT_EQ(TestCountryNames("nl").GetCountryCode(u"duitsland"), "DE");
}

TEST(CountryNamesTest, GetCountryCode_EnUsFallback) {
  TestCountryNames es_names("es");
  EXPECT_EQ(es_names.GetCountryCode(u"United States"), "US");
  EXPECT_EQ(es_names.GetCountryCode(u"united states"), "US");
  EXPECT_EQ(es_names.GetCountryCode(u"USA"), "US");
}

TEST(CountryNamesTest, GetCountryCodeForLocalizedCountryName) {
  // Initialize with the default locale.
  TestCountryNames names("en_US");
  EXPECT_EQ(names.GetCountryCodeForLocalizedCountryName(u"Armenien", "de"),
            "AM");
  // Check that there is no cache by requesting the same result twice.
  EXPECT_EQ(names.GetCountryCodeForLocalizedCountryName(u"Armenien", "de"),
            "AM");
  EXPECT_EQ(names.GetCountryCodeForLocalizedCountryName(u"Azerbeidzjan", "nl"),
            "AZ");
}

TEST(CountryNamesTest, GetCachedCountryCodeForLocalizedCountryName) {
  // Initialize with the default locale.
  TestCountryNames names("en_US");

  // Verify that the entry is not cached.
  EXPECT_FALSE(names.IsCountryNamesForLocaleCachedForTesting("de"));

  // Make a lookup of the entry that should result in a cache write.
  EXPECT_EQ(names.GetCountryCodeForLocalizedCountryName(u"Armenien", "de"),
            "AM");

  // Verify that the entry is cached.
  EXPECT_TRUE(names.IsCountryNamesForLocaleCachedForTesting("de"));
}

TEST(CountryNamesTest, GetCountryCode_EmptyString) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ(en_us_names.GetCountryCode(u""), "");
}

// Tests that native names of countries are mapped to their country codes
// even if the locale does not match (e.g. "Deutschland" is not the en_US
// representation of "Germany").
TEST(CountryNamesTest, GetCountryCode_NativeNames_Uppercase) {
  TestCountryNames en_us_names("en_US");
  // German, uppercase
  EXPECT_EQ(en_us_names.GetCountryCode(u"DEUTSCHLAND"), "DE");
  // Greek, uppercase
  EXPECT_EQ(en_us_names.GetCountryCode(u"ΕΛΛΆΔΑ"), "GR");
  // Russian, uppercase
  EXPECT_EQ(en_us_names.GetCountryCode(u"РОССИЯ"), "RU");
  // Japanese, no case, should still work.
  EXPECT_EQ(en_us_names.GetCountryCode(u"日本"), "JP");
}

// Tests that country native names are correctly mapped even in lowercase.
// The native names are stored in uppercase so this test ensures that the
// international uppercasing works as expected.
TEST(CountryNamesTest, GetCountryCode_NativeNames_Lowercase) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ(en_us_names.GetCountryCode(u"Deutschland"), "DE");
  EXPECT_EQ(en_us_names.GetCountryCode(u"España"), "ES");
  EXPECT_EQ(en_us_names.GetCountryCode(u"France"), "FR");
  EXPECT_EQ(en_us_names.GetCountryCode(u"ελλάδα"), "GR");
  EXPECT_EQ(en_us_names.GetCountryCode(u"россия"), "RU");
  // Japanese, no case, should still work.
  EXPECT_EQ(en_us_names.GetCountryCode(u"日本"), "JP");
}

// Test mapping of an empty country name to an country code.
TEST(CountryNamesTest, EmptyCountryNameHasEmptyCountryCode) {
  std::string country_code =
      TestCountryNames("en").GetCountryCode(std::u16string());
  EXPECT_TRUE(country_code.empty()) << country_code;
}

}  // namespace
}  // namespace autofill
