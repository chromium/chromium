// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Tests that GetPatterns returns an empty string  if no patterns can be found.
TEST(AutofillStructuredAddressFormatProvider, GetPatternReturnsEmpty) {
  auto* f_pattern_provider = StructuredAddressesFormatProvider::GetInstance();
  EXPECT_EQ(u"", f_pattern_provider->GetPattern(FieldType::UNKNOWN_TYPE,
                                                /*country_code=*/""));
}

// Tests that GetPatterns returns non empty patterns for home street address.
TEST(AutofillStructuredAddressFormatProvider, NonEmptyPatternsForHomeAddress) {
  auto* pattern_provider = StructuredAddressesFormatProvider::GetInstance();
  std::vector<std::string> country_codes = {"BR", "DE", "MX", "ES"};

  for (const std::string& country_code : country_codes) {
    EXPECT_NE(u"", pattern_provider->GetPattern(
                       FieldType::ADDRESS_HOME_STREET_ADDRESS, country_code));
  }

  // If the country code is not provided, there is a default value;
  EXPECT_NE(u"",
            pattern_provider->GetPattern(FieldType::ADDRESS_HOME_STREET_ADDRESS,
                                         /*country_code=*/""));
}

// Tests that GetPatterns returns non empty patterns for full name.
TEST(AutofillStructuredAddressFormatProvider, NonEmptyPatternsForFullName) {
  auto* pattern_provider = StructuredAddressesFormatProvider::GetInstance();
  EXPECT_NE(u"", pattern_provider->GetPattern(FieldType::NAME_FULL,
                                              /*country_code=*/""));

  StructuredAddressesFormatProvider::ContextInfo info;
  info.name_has_cjk_characteristics = true;

  EXPECT_NE(u"", pattern_provider->GetPattern(FieldType::NAME_FULL,
                                              /*country_code=*/"", info));
}

}  // namespace autofill
