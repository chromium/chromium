// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/string_view_rust.h"
#include "components/facilitated_payments/core/validation/payment_qr_validator_cxx.rs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

// All codes below end with a valid CRC-16/CCITT-FALSE, unless the test is about
// the CRC itself.

// Expects `code` to be validated as a code of `scheme` with `result`.
void ExpectValidation(std::string_view code,
                      PaymentQrCodeScheme scheme,
                      PaymentQrCodeValidationResult result) {
  SCOPED_TRACE(code);
  PaymentQrCodeValidation validation =
      validate_payment_qr_code(base::StringViewToRustSlice(code));
  EXPECT_EQ(validation.scheme, scheme);
  EXPECT_EQ(validation.result, result);
}

// Tests that a dynamic Pix code is classified as a dynamic Pix code.
TEST(PaymentQrValidatorTest, PixDynamicCode) {
  ExpectValidation("00020126370014br.gov.bcb.pix2515www.example.com63042003",
                   PaymentQrCodeScheme::Pix,
                   PaymentQrCodeValidationResult::ValidPixDynamic);
}

// Tests that a static Pix code is classified as a static Pix code.
TEST(PaymentQrValidatorTest, PixStaticCode) {
  ExpectValidation(
      "00020126580014br.gov.bcb.pix"
      "0136123e4567-e12b-12d1-a456-426655440000"
      "6304BB8F",
      PaymentQrCodeScheme::Pix, PaymentQrCodeValidationResult::ValidPixStatic);
}

// Tests that the Pix identifier is compared case-insensitively.
TEST(PaymentQrValidatorTest, PixCodeWithUpperCaseIdentifier) {
  ExpectValidation("00020126370014BR.GOV.BCB.PIX2515www.example.com6304D3B0",
                   PaymentQrCodeScheme::Pix,
                   PaymentQrCodeValidationResult::ValidPixDynamic);
}

// Tests that a code with the DuitNow identifier is classified as DuitNow.
TEST(PaymentQrValidatorTest, DuitNowCode) {
  ExpectValidation(
      "00020126280014A0000006150001"
      "0106890000"
      "5802MY6304DCBC",
      PaymentQrCodeScheme::DuitNow, PaymentQrCodeValidationResult::Valid);
}

// Tests that application identifiers are compared case-insensitively.
TEST(PaymentQrValidatorTest, DuitNowCodeWithLowerCaseIdentifier) {
  ExpectValidation(
      "00020126280014a0000006150001"
      "0106890000"
      "5802MY63047A4E",
      PaymentQrCodeScheme::DuitNow, PaymentQrCodeValidationResult::Valid);
}

// Tests that a PromptPay credit transfer code is classified as PromptPay.
TEST(PaymentQrValidatorTest, PromptPayCode) {
  ExpectValidation(
      "000201010212"
      "29370016A00000067701011101130066812345678"
      "53037645406150.005802TH6304C40C",
      PaymentQrCodeScheme::PromptPay, PaymentQrCodeValidationResult::Valid);
}

// Tests that a PromptPay bill payment code, as shown at checkout, is classified
// as PromptPay.
TEST(PaymentQrValidatorTest, PromptPayBillPaymentCode) {
  ExpectValidation(
      "00020101021230580016A000000677010112"
      "0115000000000000000"
      "0215000000000000000"
      "53037645802TH63044A2C",
      PaymentQrCodeScheme::PromptPay, PaymentQrCodeValidationResult::Valid);
}

// Tests that a code with the national QRIS identifier is classified as QRIS.
TEST(PaymentQrValidatorTest, QrisCode) {
  ExpectValidation(
      "00020151440014ID.CO.QRIS.WWW"
      "0215ID10200123456780303UMI"
      "53033605802ID630453A1",
      PaymentQrCodeScheme::Qris, PaymentQrCodeValidationResult::Valid);
}

// Tests that a code with only a QRIS issuer identifier is classified as QRIS.
TEST(PaymentQrValidatorTest, QrisCodeWithIssuerIdentifierOnly) {
  ExpectValidation(
      "00020126420016ID.CO.TELKOM.WWW"
      "0118936000091234567890"
      "53033605802ID6304353C",
      PaymentQrCodeScheme::Qris, PaymentQrCodeValidationResult::Valid);
}

// Tests that the QRIS identifier is compared case-insensitively.
TEST(PaymentQrValidatorTest, QrisCodeWithLowerCaseIdentifier) {
  ExpectValidation(
      "00020151370014id.co.qris.www"
      "0215ID1020012345678"
      "5802ID63046922",
      PaymentQrCodeScheme::Qris, PaymentQrCodeValidationResult::Valid);
}

// Tests that a code with the ShopeePay identifier is classified as ShopeePay.
TEST(PaymentQrValidatorTest, ShopeePayCode) {
  ExpectValidation(
      "00020101021226430017MY.COM.AIRPAY.WWW"
      "0118000000000000000000"
      "53034585802MY63049A9D",
      PaymentQrCodeScheme::ShopeePay, PaymentQrCodeValidationResult::Valid);
}

// Tests that the ShopeePay identifier is compared case-insensitively.
TEST(PaymentQrValidatorTest, ShopeePayCodeWithLowerCaseIdentifier) {
  ExpectValidation(
      "00020101021226430017my.com.airpay.www"
      "0118000000000000000000"
      "5802MY6304710E",
      PaymentQrCodeScheme::ShopeePay, PaymentQrCodeValidationResult::Valid);
}

// Tests that a MoMo code is classified as MoMo, since its MoMo template comes
// before its VietQR template.
TEST(PaymentQrValidatorTest, MoMoCode) {
  ExpectValidation(
      "00020101021226310007vn.momo"
      "0116MOMOEXAMPLE00001"
      "38280010A00000072701100006970422"
      "53037045802VN6304D2CA",
      PaymentQrCodeScheme::MoMo, PaymentQrCodeValidationResult::Valid);
}

// Tests that a VietQR code without a MoMo template is not supported.
TEST(PaymentQrValidatorTest, VietQrOnlyCode) {
  ExpectValidation(
      "00020101021238280010A000000727"
      "01100006970422"
      "5802VN6304FCED",
      PaymentQrCodeScheme::Unknown,
      PaymentQrCodeValidationResult::UnsupportedScheme);
}

// Tests that a code whose identifier is not supported is rejected.
TEST(PaymentQrValidatorTest, UnknownIdentifier) {
  ExpectValidation("00020126280015com.example.pay0105123456304FD43",
                   PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::UnsupportedScheme);
}

// Tests that a code with only a payment network data object, which carries no
// identifier, is rejected.
TEST(PaymentQrValidatorTest, PaymentNetworkOnlyCode) {
  ExpectValidation("000201041654133300890200115802SG6304EDF8",
                   PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::UnsupportedScheme);
}

// Tests that a template with an unsupported identifier is skipped, so that a
// later template can still identify the scheme.
TEST(PaymentQrValidatorTest, UnknownTemplateBeforePixTemplate) {
  ExpectValidation(
      "00020126280015com.example.pay010512345"
      "27370014br.gov.bcb.pix2515www.example.com"
      "630490D0",
      PaymentQrCodeScheme::Pix, PaymentQrCodeValidationResult::ValidPixDynamic);
}

// Tests that an identifier outside of the merchant account information
// template IDs "26" to "51" is ignored.
TEST(PaymentQrValidatorTest, PixIdentifierOutsideTemplateRange) {
  ExpectValidation("00020152370014br.gov.bcb.pix2515www.example.com63042339",
                   PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::UnsupportedScheme);
}

// Tests that a Pix code that is neither static nor dynamic is rejected.
TEST(PaymentQrValidatorTest, PixCodeOfUnknownType) {
  ExpectValidation("00020126250014br.gov.bcb.pix0203abc6304DE12",
                   PaymentQrCodeScheme::Pix,
                   PaymentQrCodeValidationResult::UnsupportedScheme);
}

// Tests that a code whose CRC does not match is rejected.
TEST(PaymentQrValidatorTest, WrongCrc) {
  ExpectValidation("00020126370014br.gov.bcb.pix2515www.example.com63042002",
                   PaymentQrCodeScheme::Pix,
                   PaymentQrCodeValidationResult::FailedCrcCheck);
}

// Tests that a failing code is still attributed to its scheme, so that
// failures can be measured per scheme.
TEST(PaymentQrValidatorTest, WrongCrcKeepsScheme) {
  ExpectValidation(
      "00020126280014A0000006150001"
      "0106890000"
      "5802MY6304DCBD",
      PaymentQrCodeScheme::DuitNow,
      PaymentQrCodeValidationResult::FailedCrcCheck);
}

// Tests that the CRC is compared case-insensitively.
TEST(PaymentQrValidatorTest, LowerCaseCrc) {
  ExpectValidation(
      "00020126580014br.gov.bcb.pix"
      "0136123e4567-e12b-12d1-a456-426655440000"
      "6304bb8f",
      PaymentQrCodeScheme::Pix, PaymentQrCodeValidationResult::ValidPixStatic);
}

// Tests that inputs that are not EMV Merchant-Presented QR codes are rejected.
TEST(PaymentQrValidatorTest, NotPaymentCode) {
  ExpectValidation("", PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::NotPaymentCode);
  ExpectValidation("https://www.example.com", PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::NotPaymentCode);
  ExpectValidation("\xff\xfe", PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::NotPaymentCode);
}

// Tests that a code that does not begin with the payload format indicator is
// rejected.
TEST(PaymentQrValidatorTest, MissingPayloadFormatIndicator) {
  ExpectValidation("01021226370014br.gov.bcb.pix2515www.example.com630416BD",
                   PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::NotPaymentCode);
}

// Tests that a code without a CRC is rejected.
TEST(PaymentQrValidatorTest, MissingCrc) {
  ExpectValidation("00020126370014br.gov.bcb.pix2515www.example.com5802BR",
                   PaymentQrCodeScheme::Pix,
                   PaymentQrCodeValidationResult::FailedTlvParsing);
}

// Tests that a code whose CRC is not the final data object is rejected.
TEST(PaymentQrValidatorTest, NonFinalCrc) {
  ExpectValidation(
      "00020126370014br.gov.bcb.pix"
      "2515www.example.com630420035802BR",
      PaymentQrCodeScheme::Pix,
      PaymentQrCodeValidationResult::FailedTlvParsing);
}

// Tests that a code with a truncated data object is rejected.
TEST(PaymentQrValidatorTest, TruncatedDataObject) {
  ExpectValidation("00020126370014br.gov.bcb.pix2515www.exampl",
                   PaymentQrCodeScheme::Unknown,
                   PaymentQrCodeValidationResult::FailedTlvParsing);
}

// Tests that a code with an empty additional data field template is rejected.
TEST(PaymentQrValidatorTest, EmptyAdditionalDataFieldTemplate) {
  ExpectValidation(
      "00020126370014br.gov.bcb.pix"
      "2515www.example.com62006304BBBE",
      PaymentQrCodeScheme::Pix,
      PaymentQrCodeValidationResult::EmptyAdditionalDataFieldTemplate);
}

// Tests that a wrong CRC is reported before the content is checked.
TEST(PaymentQrValidatorTest, WrongCrcTakesPrecedence) {
  ExpectValidation(
      "00020126370014br.gov.bcb.pix"
      "2515www.example.com62006304BBBF",
      PaymentQrCodeScheme::Pix, PaymentQrCodeValidationResult::FailedCrcCheck);
  ExpectValidation("00020126250014br.gov.bcb.pix0203abc6304DE13",
                   PaymentQrCodeScheme::Pix,
                   PaymentQrCodeValidationResult::FailedCrcCheck);
}

}  // namespace
}  // namespace payments::facilitated
