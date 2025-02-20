// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/country_info.h"

#include <stddef.h>

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

constexpr char kAppLocaleUS[] = "US";

// Tests that setting and getting the country code via `[Set|Get]RawInfo`
// works as expected.
TEST(CountryInfoTest, RawMethods) {
  CountryInfo country;
  ASSERT_TRUE(country.GetRawInfo(ADDRESS_HOME_COUNTRY).empty());

  // `SetRawInfo` doesn't allow setting the country code via country name.
  country.SetRawInfo(ADDRESS_HOME_COUNTRY, u"Germany");
  EXPECT_TRUE(country.GetRawInfo(ADDRESS_HOME_COUNTRY).empty());

  // `SetRawInfo` allows setting the country code directly.
  country.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  EXPECT_EQ(country.GetRawInfo(ADDRESS_HOME_COUNTRY), u"DE");
}

// Tests that we can read the country name of an object constructed with country
// name.
TEST(CountryInfoTest, SetNameReadName) {
  CountryInfo country;

  // `SetRawInfo` doesn't allow setting the country code via country name.
  country.SetInfo(ADDRESS_HOME_COUNTRY, u"Germany", kAppLocaleUS);
  EXPECT_EQ(country.GetInfo(ADDRESS_HOME_COUNTRY, kAppLocaleUS), u"Germany");
}

// Tests that we can read the country code of an object constructed with country
// name.
TEST(CountryInfoTest, SetNameReadCode) {
  CountryInfo country;

  // `SetRawInfo` doesn't allow setting the country code via country name.
  country.SetInfo(ADDRESS_HOME_COUNTRY, u"Germany", kAppLocaleUS);
  EXPECT_EQ(country.GetRawInfo(ADDRESS_HOME_COUNTRY), u"DE");
}

// Tests that we can read the country name of an object constructed with country
// code.
TEST(CountryInfoTest, SetCodeReadName) {
  CountryInfo country;

  // `SetRawInfo` doesn't allow setting the country code via country name.
  country.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  EXPECT_EQ(country.GetInfo(ADDRESS_HOME_COUNTRY, kAppLocaleUS), u"Germany");
}

}  // namespace

}  // namespace autofill
