// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/country_names_for_locale.h"

#include <string>
#include <utility>

#include "base/i18n/language_tag.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using base::ASCIIToUTF16;

// Test that the correct country code is returned for various locales.
TEST(CountryNamesForLocaleTest, GetCountryCode) {
  CountryNamesForLocale en_us_names(base::i18n::GetKnownLanguageTag("en-US"));
  EXPECT_EQ("US", en_us_names.GetCountryCode(u"United States"));

  CountryNamesForLocale de_names(base::i18n::GetKnownLanguageTag("de"));
  EXPECT_EQ("DE", de_names.GetCountryCode(u"Deutschland"));
}

// The behavior depends on the platform. On Android the locale reverts back to
// the standard locale.
#if !BUILDFLAG(IS_ANDROID)
// Test that an empty string is returned for an undetermined locale.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForEmptyLocale) {
  CountryNamesForLocale empty_locale_names(
      base::i18n::GetKnownLanguageTag("und"));
  EXPECT_EQ("", empty_locale_names.GetCountryCode(u"United States"));
}
#endif

// Test that an empty string is returned for an empty country name.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForEmptyCountryName) {
  CountryNamesForLocale de_names(base::i18n::GetKnownLanguageTag("de"));
  EXPECT_EQ("", de_names.GetCountryCode(u""));
}

// Test that an empty string is returned for an invalid country name.
TEST(CountryNamesForLocaleTest, EmptyCountryCodeForInvalidCountryName) {
  CountryNamesForLocale de_names(base::i18n::GetKnownLanguageTag("de"));
  EXPECT_EQ("", de_names.GetCountryCode(u"ThisISNotACountry"));
}

// Test that an instance is correctly constructed using the move semantics.
TEST(CountryNamesForLocaleTest, MoveConstructior) {
  // Construct a working `CountryNamesForLocale` instance.
  CountryNamesForLocale de_names(base::i18n::GetKnownLanguageTag("de"));
  EXPECT_EQ("DE", de_names.GetCountryCode(u"Deutschland"));

  // Construct another instance using the move semantics.
  CountryNamesForLocale moved_names(std::move(de_names));

  // Test that the new instance returns the correct values.
  EXPECT_EQ("DE", moved_names.GetCountryCode(u"Deutschland"));
}

}  // namespace

}  // namespace autofill
