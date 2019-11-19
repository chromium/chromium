// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/country_names.h"
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

}  // namespace

// Test mapping of localized country names to country codes.
TEST(CountryNamesTest, GetCountryCode_BasicMapping) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ("US", en_us_names.GetCountryCode(ASCIIToUTF16("United States")));
  EXPECT_EQ("CA", en_us_names.GetCountryCode(ASCIIToUTF16("Canada")));
}

TEST(CountryNamesTest, GetCountryCode_CaseInsensitiveMapping) {
  EXPECT_EQ("US", TestCountryNames("en_US").GetCountryCode(
                      ASCIIToUTF16("united states")));
}

TEST(CountryNamesTest, GetCountryCode_CodesMapToThemselves) {
  TestCountryNames en_us_names("en_US");
  TestCountryNames fr_ca_names("fr_CA");
  EXPECT_EQ("US", en_us_names.GetCountryCode(ASCIIToUTF16("US")));
  EXPECT_EQ("HU", en_us_names.GetCountryCode(ASCIIToUTF16("hu")));
  EXPECT_EQ("CA", fr_ca_names.GetCountryCode(ASCIIToUTF16("CA")));
  EXPECT_EQ("MX", fr_ca_names.GetCountryCode(ASCIIToUTF16("mx")));
}

TEST(CountryNamesTest, GetCountryCode_BasicSynonyms) {
  TestCountryNames en_us_names("en_US");
  EXPECT_EQ("US", en_us_names.GetCountryCode(
                      ASCIIToUTF16("United States of America")));
  EXPECT_EQ("US", en_us_names.GetCountryCode(ASCIIToUTF16("USA")));
}

TEST(CountryNamesTest, GetCountryCode_OtherLocales) {
  EXPECT_EQ("US", TestCountryNames("es").GetCountryCode(
                      ASCIIToUTF16("Estados Unidos")));
  EXPECT_EQ("IT",
            TestCountryNames("it").GetCountryCode(ASCIIToUTF16("Italia")));
  EXPECT_EQ("DE",
            TestCountryNames("nl").GetCountryCode(ASCIIToUTF16("duitsland")));
}

TEST(CountryNamesTest, GetCountryCode_EnUsFallback) {
  TestCountryNames es_names("es");
  EXPECT_EQ("US", es_names.GetCountryCode(ASCIIToUTF16("United States")));
  EXPECT_EQ("US", es_names.GetCountryCode(ASCIIToUTF16("united states")));
  EXPECT_EQ("US", es_names.GetCountryCode(ASCIIToUTF16("USA")));
}

// Test mapping of empty country name to country code.
TEST(CountryNamesTest, EmptyCountryNameHasEmptyCountryCode) {
  std::string country_code =
      TestCountryNames("en").GetCountryCode(base::string16());
  EXPECT_TRUE(country_code.empty()) << country_code;
}

}  // namespace autofill
