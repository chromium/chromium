// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/pix_code_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

TEST(PixCodeValidatorTest, ValidCode) {
  EXPECT_TRUE(IsValidPixCode("00020126180014br.gov.bcb.pix63041D3D"));
}

TEST(PixCodeValidatorTest, EmptyStringNotValid) {
  EXPECT_FALSE(IsValidPixCode(""));
}

TEST(PixCodeValidatorTest, LastSectionLengthTooLong) {
  EXPECT_FALSE(IsValidPixCode("00020126180014br.gov.bcb.pix63051D3D"));
}

TEST(PixCodeValidatorTest, SectionHeaderIsNotADigit) {
  EXPECT_FALSE(IsValidPixCode("000A0126180014br.gov.bcb.pix63041D3D"));
}

TEST(PixCodeValidatorTest, SectionTruncatedTooShort) {
  EXPECT_FALSE(IsValidPixCode("00020126180014br.gov.bcb.pix63041D3"));
}

TEST(PixCodeValidatorTest, SectionHeaderTruncatedTooShort) {
  EXPECT_FALSE(IsValidPixCode("00020126180014br.gov.bcb.pix630"));
}

TEST(PixCodeValidatorTest, MerchantAccountInformationIsEmpty) {
  EXPECT_FALSE(IsValidPixCode("000201260063041D3D"));
}

TEST(PixCodeValidatorTest, MerchantAccountInformationIsNotValid) {
  EXPECT_FALSE(IsValidPixCode("00020126030014br.gov.bcb.pix63041D3D"));
}

TEST(PixCodeValidatorTest, InvalidPixCodeIndicator) {
  EXPECT_FALSE(IsValidPixCode("00020126180014br.gov.bcb.PIX63041D3D"));
}

TEST(PixCodeValidatorTest, EmptyAdditionalDataSection) {
  EXPECT_FALSE(IsValidPixCode("00020126180014br.gov.bcb.pix620063041D3D"));
}

TEST(PixCodeValidatorTest, LastSectionIdIsNotCrc16) {
  EXPECT_FALSE(IsValidPixCode("00020126180014br.gov.bcb.pix64041D3D"));
}

TEST(PixCodeValidatorTest, FirstSectionIsNotPayloadIndicator) {
  EXPECT_FALSE(IsValidPixCode("01020126180014br.gov.bcb.pix63041D3D"));
}

TEST(PixCodeValidatorTest, NoMerchantAccountInformationSection) {
  EXPECT_FALSE(IsValidPixCode("00020163041D3D"));
}

TEST(PixCodeValidatorTest, NoPixCodeIndicator) {
  EXPECT_FALSE(IsValidPixCode("000201261801020063041D3D"));
}

}  // namespace
}  // namespace payments::facilitated
