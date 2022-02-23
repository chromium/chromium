// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names_for_locale.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {

class TestCountryNamesForLocale : public CountryNamesForLocale {
 public:
  explicit TestCountryNamesForLocale(const std::string& locale_name)
      : CountryNamesForLocale(locale_name) {}

  TestCountryNamesForLocale(TestCountryNamesForLocale&& source)
      : CountryNamesForLocale(std::move(source)) {}

  ~TestCountryNamesForLocale() = default;
};

}  // namespace

// Test that the correct country code is returned for various locales.
TEST(CountryNamesForLocaleTest, GetCountryCode) {
  TestCountryNamesForLocale en_us_names("en_US");
  EXPECT_EQ("US", en_us_names.GetCountryCode(u"United States"));

  TestCountryNamesForLocale de_names("de");
  EXPECT_EQ("DE", de_names.GetCountryCode(u"Deutschland"));
}

// Test that supplying an non-empty but invalid locale reverts back to 'en_US'
// localized names.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForInvalidLocale) {
  TestCountryNamesForLocale not_a_locale_names("not_a_locale");

  // The creation of an non-empty invalid locale reverts back to "en_US".
  EXPECT_EQ("US", not_a_locale_names.GetCountryCode(u"United States"));
}

// The behavior depends on the platform. On Android the locale reverts back to
// the standard locale.
#if !BUILDFLAG(IS_ANDROID)
// Test that an empty string is returned for an empty locale.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForEmptyLocale) {
  TestCountryNamesForLocale empty_locale_names("");
  EXPECT_EQ("", empty_locale_names.GetCountryCode(u"United States"));
}
#endif

// Test that an empty string is returned for an empty country name.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForEmptyCountryName) {
  TestCountryNamesForLocale de_names("de");
  EXPECT_EQ("", de_names.GetCountryCode(u""));
}

// Test that an empty string is returned for an invalid country name.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForInvalidCountryName) {
  TestCountryNamesForLocale de_names("de");
  EXPECT_EQ("", de_names.GetCountryCode(u"ThisISNotACountry"));
}

// Test that an instance is correctly constructed using the move semantics.
TEST(CountryNamesForLocaleTest, MoveConstructior) {
  // Construct a working |CountryNamesForLocale| instance.
  TestCountryNamesForLocale de_names("de");
  EXPECT_EQ("DE", de_names.GetCountryCode(u"Deutschland"));

  // Construct another instance using the move semantics.
  TestCountryNamesForLocale moved_names(std::move(de_names));

  // Test that the new instance returns the correct values.
  EXPECT_EQ("DE", moved_names.GetCountryCode(u"Deutschland"));
}
}  // namespace autofill
