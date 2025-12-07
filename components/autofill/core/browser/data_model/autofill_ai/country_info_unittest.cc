// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/country_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

constexpr char kAppLocaleUS[] = "US";

// Tests that we can read the country name of an object constructed with country
// name.
TEST(CountryInfoTest, SetNameReadName) {
  CountryInfo country;
  country.SetCountryFromCountryName(u"Germany", kAppLocaleUS);
  EXPECT_EQ(country.GetCountryName(kAppLocaleUS), u"Germany");
}

// Tests that we can read the country code of an object constructed with country
// name.
TEST(CountryInfoTest, SetNameReadCode) {
  CountryInfo country;
  country.SetCountryFromCountryName(u"Germany", kAppLocaleUS);
  EXPECT_EQ(country.GetCountryCode(), "DE");
}

// Tests that we can read the country name of an object constructed with country
// code.
TEST(CountryInfoTest, SetCodeReadName) {
  CountryInfo country;
  country.SetCountryFromCountryCode(u"DE");
  EXPECT_EQ(country.GetCountryName(kAppLocaleUS), u"Germany");
}

// Tests that we can read the country code of an object constructed with country
// code.
TEST(CountryInfoTest, SetCodeReadCode) {
  CountryInfo country;
  country.SetCountryFromCountryCode(u"DE");
  EXPECT_EQ(country.GetCountryCode(), "DE");
}

// Tests that we can construct an object with a lowercase country code, but
// reading the country code would return the uppercase version of it.
TEST(CountryInfoTest, SetCodeLowercaseReadCodeUppercase) {
  CountryInfo country;
  country.SetCountryFromCountryCode(u"de");
  EXPECT_EQ(country.GetCountryCode(), "DE");
}

}  // namespace

}  // namespace autofill
