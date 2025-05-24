// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/country_codes/country_codes.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace country_codes {

namespace {
// A subset of valid country codes.
constexpr char kCountryCodeList[][3] = {"AE", "AU", "BR", "CA", "CN", "DE",
                                        "FR", "GB", "IN", "JP", "MX", "NZ",
                                        "PL", "US", "VI", "ZA"};

TEST(CountryCodesTest, ConvertToAndFromCountryCode) {
  // A subset of valid country codes.
  for (auto* code : kCountryCodeList) {
    // Lowercase values are not valid.
    ASSERT_FALSE(CountryId(base::ToLowerASCII(code)).IsValid());

    // Uppercase values are valid and properly reflected.
    ASSERT_TRUE(CountryId(code).IsValid());
    ASSERT_EQ(code, CountryId(code).CountryCode());
  }
}

TEST(CountryCodesTest, InvalidCountryCode) {
  ASSERT_FALSE(CountryId("Z").IsValid());
  ASSERT_FALSE(CountryId("ZZZ").IsValid());
  ASSERT_FALSE(CountryId("A1").IsValid());
  ASSERT_FALSE(CountryId("1A").IsValid());
  ASSERT_FALSE(CountryId("12").IsValid());
}

TEST(CountryCodesTest, DeserializeInvalidCodes) {
  ASSERT_FALSE(CountryId::Deserialize(1 << 16).IsValid());
  ASSERT_FALSE(CountryId::Deserialize(-5).IsValid());
  ASSERT_FALSE(CountryId::Deserialize(0x1234).IsValid());
  ASSERT_FALSE(CountryId::Deserialize('0' << 8 | '1').IsValid());
}

TEST(CountryCodesTest, DeserializeValidCodes) {
  for (auto* code : kCountryCodeList) {
    ASSERT_EQ(CountryId(code),
              CountryId::Deserialize(CountryId(code).Serialize()));
  }
}

}  // namespace

}  // namespace country_codes
