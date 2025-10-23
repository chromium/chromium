// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/validation/pix_code_validator.h"

#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

TEST(PixCodeValidatorTest, ValidDynamicCode) {
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F"),
            mojom::PixQrCodeType::kDynamic);
}

TEST(PixCodeValidatorTest, ValidDynamicCodeWithSomeUpperCaseLetters) {
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014Br.gOv.BcB.piX2515www.example.com6304EA3F"),
            mojom::PixQrCodeType::kDynamic);
}

TEST(PixCodeValidatorTest, StaticCode) {
  // Code is invalid because merchant account identifier section
  // 26270014br.gov.bcb.pix0105ABCDE does not contain a section for dynamic url
  // with id 25.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126270014br.gov.bcb.pix0105ABCDE63041D3D"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, EmptyStringNotValid) {
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(""),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, LastSectionLengthTooLong) {
  // Code is invalid because the last section 63051D3D has the length specified
  // as 05 which is longer than the string succeeding it (1D3D).
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014br.gov.bcb.pix2514www.example.com63051D3D"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, SectionHeaderIsNotADigit) {
  // Code is invalid because the section 000A01 does not have the first 4
  // characters as digits.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "000A0126370014br.gov.bcb.pix2514www.example.com6304EA3F"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, LastSectionLengthTooShort) {
  // Code is invalid because the last section 63021D3 has the length specified
  // as 02 which is shorter than the length of the string succeeding it (1D3).
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014br.gov.bcb.pix2514www.example.com63021D3"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, SectionHeaderTruncatedTooShort) {
  // Code is invalid because the last section 630 doesn't have the minimum
  // length of 4 characters.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014br.gov.bcb.pix2514www.example.com630"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, MerchantAccountInformationIsEmpty) {
  // Code is invalid because the section 2600 has a length of 00.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType("000201260063041D3D"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, MerchantAccountInformationIsNotValid) {
  // Code is invalid because the merchant account information section 2603001
  // does not contain the Pix code indicator 0014br.gov.bcb.pix.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126030014br.gov.bcb.pix2514www.example.com6304EA3F"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, InvalidPixCodeIndicator) {
  // Code is invalid because the Pix code indicator is 0014br.gov.bcb.pxi
  // instead 0014br.gov.bcb.pix.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014br.gov.bcb.pxi2514www.example.com6304EA3F"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, EmptyAdditionalDataSection) {
  // Code is invalid because the additional data section 6200 has a length of
  // 00.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014br.gov.bcb.pix2514www.example.com620063041D3D"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, LastSectionIdIsNotCrc16) {
  // Code is invalid because the last section 64041D3D has an id 64 instead
  // of 64.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "00020126370014br.gov.bcb.pix2514www.example.com64041D3D"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, FirstSectionIsNotPayloadIndicator) {
  // Code is invalid because the first section 010201 has an id 01 instead of
  // 00.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType(
                "01020126370014br.gov.bcb.pix2514www.example.com6304EA3F"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, NoMerchantAccountInformationSection) {
  // Code is invalid because there is no merchant account information section
  // with id 26.
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType("00020163041D3D"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, NoPixCodeIndicator) {
  // Code is invalid because the merchant account information section
  // 261801020063041D3D does not contain the Pix code indicator
  // 0014br.gov.bcb.pix .
  EXPECT_EQ(PixCodeValidator::GetPixQrCodeType("000201261801020063041D3D"),
            mojom::PixQrCodeType::kInvalid);
}

TEST(PixCodeValidatorTest, ContainsPixCodeIdentifier) {
  std::string pixCodeIndicatorLowercase = "0014br.gov.bcb.pix";
  EXPECT_TRUE(PixCodeValidator::ContainsPixIdentifier(
      base::StrCat({"0002012637", pixCodeIndicatorLowercase,
                    "2514www.example.com64041D3D"})));
}

TEST(PixCodeValidatorTest, ContainsPixCodeIdentifier_MixedCase) {
  std::string pixCodeIndicatorLowercase = "0014BR.GoV.Bcb.PIX";
  EXPECT_TRUE(PixCodeValidator::ContainsPixIdentifier(
      base::StrCat({"0002012637", pixCodeIndicatorLowercase,
                    "2514www.example.com64041D3D"})));
}

TEST(PixCodeValidatorTest, DoesNotContainsPixCodeIdentifier) {
  EXPECT_FALSE(PixCodeValidator::ContainsPixIdentifier("example.com64041D3D"));
}

}  // namespace
}  // namespace payments::facilitated
