// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/company_info.h"

#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Tests that setting and getting raw info for `COMPANY_NAME` works correctly.
TEST(CompanyInfoTest, SetRawInfo) {
  CompanyInfo company;
  company.SetRawInfo(COMPANY_NAME, u"Google");
  EXPECT_EQ(company.GetRawInfo(COMPANY_NAME), u"Google");
}

// Tests that company names are validated to filter out birth years and social
// titles.
TEST(CompanyInfoTest, IsValid) {
  auto set_and_validate = [](const std::u16string& company_name) {
    CompanyInfo company;
    company.SetRawInfo(COMPANY_NAME, company_name);
    return company.IsValid();
  };

  EXPECT_TRUE(set_and_validate(u"Google"));
  EXPECT_TRUE(set_and_validate(u"1818"));
  EXPECT_FALSE(set_and_validate(u"1987"));
  EXPECT_FALSE(set_and_validate(u"2019"));
  EXPECT_TRUE(set_and_validate(u"2345"));
  EXPECT_TRUE(set_and_validate(u"It was 1987."));
  EXPECT_TRUE(set_and_validate(u"1987 was the year."));
  EXPECT_FALSE(set_and_validate(u"Mr"));
  EXPECT_FALSE(set_and_validate(u"Mr."));
  EXPECT_FALSE(set_and_validate(u"Mrs"));
  EXPECT_FALSE(set_and_validate(u"Mrs."));
  EXPECT_TRUE(set_and_validate(u"Mr. & Mrs."));
  EXPECT_TRUE(set_and_validate(u"Mr. & Mrs. Smith"));
  EXPECT_FALSE(set_and_validate(u"Frau"));
  EXPECT_TRUE(set_and_validate(u"Frau Doktor"));
  EXPECT_FALSE(set_and_validate(u"Herr"));
  EXPECT_FALSE(set_and_validate(u"Mme"));
  EXPECT_FALSE(set_and_validate(u"Ms"));
  EXPECT_FALSE(set_and_validate(u"Dr"));
  EXPECT_FALSE(set_and_validate(u"Dr."));
  EXPECT_FALSE(set_and_validate(u"Prof"));
  EXPECT_FALSE(set_and_validate(u"Prof."));
}

}  // namespace
}  // namespace autofill
