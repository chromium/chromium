// Copyright 2020 The Chromium Authors
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

// Test that the correct country code is returned for various locales.
TEST(CountryNamesForLocaleTest, GetCountryCode) {
  CountryNamesForLocale en_us_names("en_US");
  EXPECT_EQ("US", en_us_names.GetCountryCode(u"United States"));

  CountryNamesForLocale de_names("de");
  EXPECT_EQ("DE", de_names.GetCountryCode(u"Deutschland"));
}

// Test that supplying an non-empty but invalid locale reverts back to 'en_US'
// localized names.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForInvalidLocale) {
  CountryNamesForLocale not_a_locale_names("not_a_locale");

  // The creation of an non-empty invalid locale reverts back to "en_US".
  EXPECT_EQ("US", not_a_locale_names.GetCountryCode(u"United States"));
}

// The behavior depends on the platform. On Android the locale reverts back to
// the standard locale.
#if !BUILDFLAG(IS_ANDROID)
// TODO:(crbug.com/1456465) Re-enable test for iOS
// In iOS17, NSLocale's internal implementation was modified resulting in
// redefined behavior for existing functions. As a result,
// `l10n_util::GetDisplayNameForCountry` no longer produces the same output in
// iOS17 as previous versions.
#if BUILDFLAG(IS_IOS)
#define MAYBE_EmptyCountryCodeForEmptyLocale \
  DISABLED_EmptyCountryCodeForEmptyLocale
#else
#define MAYBE_EmptyCountryCodeForEmptyLocale EmptyCountryCodeForEmptyLocale
#endif
// Test that an empty string is returned for an empty locale.
TEST(CountryNamesForLocaleTest, MAYBE_EmptyCountryCodeForEmptyLocale) {
  CountryNamesForLocale empty_locale_names("");
  EXPECT_EQ("", empty_locale_names.GetCountryCode(u"United States"));
}
#endif

// Test that an empty string is returned for an empty country name.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForEmptyCountryName) {
  CountryNamesForLocale de_names("de");
  EXPECT_EQ("", de_names.GetCountryCode(u""));
}

// Test that an empty string is returned for an invalid country name.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForInvalidCountryName) {
  CountryNamesForLocale de_names("de");
  EXPECT_EQ("", de_names.GetCountryCode(u"ThisISNotACountry"));
}

// Test that an instance is correctly constructed using the move semantics.
TEST(CountryNamesForLocaleTest, MoveConstructior) {
  // Construct a working |CountryNamesForLocale| instance.
  CountryNamesForLocale de_names("de");
  EXPECT_EQ("DE", de_names.GetCountryCode(u"Deutschland"));

  // Construct another instance using the move semantics.
  CountryNamesForLocale moved_names(std::move(de_names));

  // Test that the new instance returns the correct values.
  EXPECT_EQ("DE", moved_names.GetCountryCode(u"Deutschland"));
}

}  // namespace autofill
