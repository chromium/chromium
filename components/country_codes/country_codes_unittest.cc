// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "components/country_codes/country_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace country_codes {

namespace {

TEST(CountryCodesTest, ConvertToAndFromCountryCode) {
  // A subset of valid country codes.
  const char kCountryCodeList[][3] = {"AE", "AU", "BR", "CA", "CN", "DE",
                                      "FR", "GB", "IN", "JP", "MX", "NZ",
                                      "PL", "US", "VI", "ZA"};

  for (auto* code : kCountryCodeList) {
    ASSERT_EQ(code, CountryIDToCountryString(CountryStringToCountryID(code)));
  }
}

TEST(CountryCodesTest, InvalidCountryCode) {
  // This code is invalid because it contains 3 characters.
  constexpr char kInvalidCountryCode[] = "ZZZ";

  // Ensure the component-defined constant is converted to the "unknown" value.
  ASSERT_EQ(kCountryIDUnknown, CountryStringToCountryID(kCountryCodeUnknown));

  // Fake country codes should also produce an error value.
  ASSERT_EQ(kCountryIDUnknown, CountryStringToCountryID(kInvalidCountryCode));
}

TEST(CountryCodesTest, InvalidCountryID) {
  // This ID is invalid because more than the bottom 16 bits are set.
  constexpr int kInvalidCountryIDExtraBits = 1 << 16;

  // Negative IDs are also invalid.
  constexpr int kInvalidCountryIDNegative = -5;

  // Ensure the component-defined constant is converted to the "unknown" value.
  ASSERT_EQ(kCountryCodeUnknown, CountryIDToCountryString(kCountryIDUnknown));

  // Fake country IDs should also produce an error value.
  ASSERT_EQ(kCountryCodeUnknown,
            CountryIDToCountryString(kInvalidCountryIDExtraBits));
  ASSERT_EQ(kCountryCodeUnknown,
            CountryIDToCountryString(kInvalidCountryIDNegative));
}

}  // namespace

}  // namespace country_codes
