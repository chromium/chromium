// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/validation/pix_code_validator.h"

#include <stdint.h>

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_view_rust.h"
#include "components/facilitated_payments/core/validation/pix_validator_cxx.rs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

enum class ValidatorImpl {
  kCxx,
  kRust,
};

mojom::PixQrCodeType RustToMojo(PixQrCodeResult result) {
  switch (result) {
    case PixQrCodeResult::Dynamic:
      return mojom::PixQrCodeType::kDynamic;
    case PixQrCodeResult::Static:
      return mojom::PixQrCodeType::kStatic;
    // All other enumerator values are errors
    default:
      return mojom::PixQrCodeType::kInvalid;
  }
}

class PixCodeValidatorTest : public ::testing::TestWithParam<ValidatorImpl> {
 public:
  static ::testing::AssertionResult CheckPixQrCodeResult(
      std::string_view code,
      PixQrCodeResult expected_result) {
    switch (GetParam()) {
      case ValidatorImpl::kCxx: {
        mojom::PixQrCodeType actual_result =
            PixCodeValidator::GetPixQrCodeType(code);
        mojom::PixQrCodeType mapped_expected_result =
            RustToMojo(expected_result);
        if (actual_result == mapped_expected_result) {
          return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure()
               << "Expected " << static_cast<uint32_t>(mapped_expected_result)
               << " but got " << static_cast<uint32_t>(actual_result);
      }
      case ValidatorImpl::kRust: {
        PixQrCodeResult actual_result =
            get_pix_qr_code_type(base::StringViewToRustSlice(code));
        if (actual_result == expected_result) {
          return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure()
               << "Expected " << static_cast<uint32_t>(expected_result)
               << " but got " << static_cast<uint32_t>(actual_result);
      }
    }
  }
};

TEST_P(PixCodeValidatorTest, ValidDynamicCode) {
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      PixQrCodeResult::Dynamic));
}

TEST_P(PixCodeValidatorTest, ValidDynamicCodeWithSomeUpperCaseLetters) {
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126370014Br.gOv.BcB.piX2515www.example.com6304EA3F",
      PixQrCodeResult::Dynamic));
}

TEST_P(PixCodeValidatorTest, StaticCode) {
  EXPECT_TRUE(
      CheckPixQrCodeResult("00020126270014br.gov.bcb.pix0105ABCDE63041D3D",
                           PixQrCodeResult::Static));
}

TEST_P(PixCodeValidatorTest, DynamicAndStatic) {
  // If a dynamic section is encountered first in the merchant account
  // information section, treat the code as dynamic.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126460014br.gov.bcb.pix2515www.example.com0105ABCDE6304EA3F",
      PixQrCodeResult::Dynamic));

  // Check that this is still the case when split across multiple account
  // information sections.
  EXPECT_TRUE(
      CheckPixQrCodeResult("00020126370014br.gov.bcb.pix2515www.example."
                           "com26270014br.gov.bcb.pix0105ABCDE6304EA3F",
                           PixQrCodeResult::Dynamic));
}

TEST_P(PixCodeValidatorTest, StaticAndDynamic) {
  // If a static section is encountered first in the merchant account
  // information section, treat the code as static.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126460014br.gov.bcb.pix0105ABCDE2515www.example.com6304EA3F",
      PixQrCodeResult::Static));

  // Check that this is still the case when split across multiple account
  // information sections.
  EXPECT_TRUE(
      CheckPixQrCodeResult("00020126270014br.gov.bcb.pix0105ABCDE26370014br."
                           "gov.bcb.pix2515www.example.com6304EA3F",
                           PixQrCodeResult::Static));
}

TEST_P(PixCodeValidatorTest, EmptyStringNotValid) {
  EXPECT_TRUE(CheckPixQrCodeResult("", PixQrCodeResult::NotPaymentCode));
}

TEST_P(PixCodeValidatorTest, LastSectionLengthTooLong) {
  // Code is invalid because the last section 63051D3D has the length specified
  // as 05 which is longer than the string succeeding it (1D3D).
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126370014br.gov.bcb.pix2515www.example.com63051D3D",
      PixQrCodeResult::InvalidMerchantPresentedCode));
}

TEST_P(PixCodeValidatorTest, SectionHeaderIsNotADigit) {
  // Code is invalid because the section 000A01 does not have the first 4
  // characters as digits.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "000A0126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      PixQrCodeResult::NotPaymentCode));
}

TEST_P(PixCodeValidatorTest, LastSectionLengthTooShort) {
  // Code is invalid because the last section 63021D3 has the length specified
  // as 02 which is shorter than the length of the string succeeding it (1D3).
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126370014br.gov.bcb.pix2515www.example.com63021D3",
      PixQrCodeResult::NonFinalCrc));
}

TEST_P(PixCodeValidatorTest, SectionHeaderTruncatedTooShort) {
  // Code is invalid because the last section 630 doesn't have the minimum
  // length of 4 characters.
  EXPECT_TRUE(
      CheckPixQrCodeResult("00020126370014br.gov.bcb.pix2515www.example.com630",
                           PixQrCodeResult::InvalidMerchantPresentedCode));
}

TEST_P(PixCodeValidatorTest, MerchantAccountInformationIsEmpty) {
  // Code is invalid because the section 2600 has a length of 00.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "000201260063041D3D", PixQrCodeResult::InvalidMerchantPresentedCode));
}

TEST_P(PixCodeValidatorTest, MerchantAccountInformationIsNotValid) {
  // Code is invalid because the merchant account information section 2629
  // does not contain the Pix code indicator 0014br.gov.bcb.pix.
  EXPECT_TRUE(
      CheckPixQrCodeResult("00020126292515www.example.com6304EA3F",
                           PixQrCodeResult::InvalidMerchantPresentedCode));
}

TEST_P(PixCodeValidatorTest, InvalidPixCodeIndicator) {
  // Code is invalid because the Pix code indicator is 0014br.gov.bcb.pxi
  // instead 0014br.gov.bcb.pix.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126370014br.gov.bcb.pxi2515www.example.com6304EA3F",
      PixQrCodeResult::NonPixMerchantPresentedCode));
}

TEST_P(PixCodeValidatorTest, EmptyAdditionalDataSection) {
  // Code is invalid because the additional data section 6200 has a length of
  // 00.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126370014br.gov.bcb.pix2515www.example.com620063041D3D",
      PixQrCodeResult::EmptyAdditionalDataFieldTemplate));
}

TEST_P(PixCodeValidatorTest, LastSectionIdIsNotCrc16) {
  // Code is invalid because the last section 64041D3D has an id 64 instead
  // of 63.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020126370014br.gov.bcb.pix2515www.example.com64041D3D",
      PixQrCodeResult::InvalidMerchantPresentedCode));
}

TEST_P(PixCodeValidatorTest, FirstSectionIsNotPayloadIndicator) {
  // Code is invalid because the first section 010201 has an id 01 instead of
  // 00.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "01020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      PixQrCodeResult::MissingPayloadFormatIndicator));
}

TEST_P(PixCodeValidatorTest, NoMerchantAccountInformationSection) {
  // Code is invalid because there is no merchant account information section
  // with id 26.
  EXPECT_TRUE(CheckPixQrCodeResult(
      "00020163041D3D", PixQrCodeResult::NonPixMerchantPresentedCode));
}

TEST_P(PixCodeValidatorTest, NoPixCodeIndicator) {
  // Code is invalid because the merchant account information section
  // 261801020063041D3D does not contain the Pix code indicator
  // 0014br.gov.bcb.pix .
  EXPECT_TRUE(
      CheckPixQrCodeResult("000201261801020063041D3D",
                           PixQrCodeResult::InvalidMerchantPresentedCode));
}

TEST(PixCodeValidatorCxxTest, ContainsPixCodeIdentifier) {
  constexpr char kPixCodeIndicatorLowercase[] = "0014br.gov.bcb.pix";
  EXPECT_TRUE(PixCodeValidator::ContainsPixIdentifier(
      base::StrCat({"0002012637", kPixCodeIndicatorLowercase,
                    "2515www.example.com64041D3D"})));
}

TEST(PixCodeValidatorCxxTest, ContainsPixCodeIdentifier_MixedCase) {
  constexpr char kPixCodeIndicatorLowercase[] = "0014BR.GoV.Bcb.PIX";
  EXPECT_TRUE(PixCodeValidator::ContainsPixIdentifier(
      base::StrCat({"0002012637", kPixCodeIndicatorLowercase,
                    "2515www.example.com64041D3D"})));
}

TEST(PixCodeValidatorCxxTest, DoesNotContainsPixCodeIdentifier) {
  EXPECT_FALSE(PixCodeValidator::ContainsPixIdentifier("example.com64041D3D"));
}

INSTANTIATE_TEST_SUITE_P(,
                         PixCodeValidatorTest,
                         ::testing::Values(ValidatorImpl::kCxx,
                                           ValidatorImpl::kRust));

}  // namespace
}  // namespace payments::facilitated
