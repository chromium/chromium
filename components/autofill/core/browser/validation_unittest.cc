// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/validation.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {

struct ExpirationDate {
  const char* year;
  const char* month;
};

struct IntExpirationDate {
  const int year;
  const int month;
};

struct SecurityCodeCardTypePair {
  const char* security_code;
  const char* card_type;
};

// From https://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
const char* const kValidNumbers[] = {
    "378282246310005",     "3714 4963 5398 431",  "3787-3449-3671-000",
    "5610591081018250",    "3056 9309 0259 04",   "3852-0000-0232-37",
    "6011111111111117",    "6011 0009 9013 9424", "3530-1113-3330-0000",
    "3566002020360505",
    "5555 5555 5555 4444",  // Mastercard.
    "5105-1051-0510-5100",
    "4111111111111111",  // Visa.
    "4012 8888 8888 1881", "4222-2222-2222-2",    "5019717010103742",
    "6331101999990016",    "6247130048162403",
    "4532261615476013542",  // Visa, 19 digits.
    "6362970000457013",     // Elo
};
const char* const kInvalidNumbers[] = {
  "4111 1111 112", /* too short */
  "41111111111111111115", /* too long */
  "4111-1111-1111-1110", /* wrong Luhn checksum */
  "3056 9309 0259 04aa", /* non-digit characters */
};
const char kCurrentDate[]="1 May 2013";
const IntExpirationDate kValidCreditCardIntExpirationDate[] = {
  { 2013, 5 },  // Valid month in current year.
  { 2014, 1 },  // Any month in next year.
  { 2014, 12 },  // Edge condition.
};
const IntExpirationDate kInvalidCreditCardIntExpirationDate[] = {
  { 2013, 4 },  // Previous month in current year.
  { 2012, 12 },  // Any month in previous year.
  { 2015, 13 },  // Not a real month.
  { 2015, 0 },  // Zero is legal in the CC class but is not a valid date.
};
const SecurityCodeCardTypePair kValidSecurityCodeCardTypePairs[] = {
    {"323", kGenericCard},           // 3-digit CSC.
    {"3234", kAmericanExpressCard},  // 4-digit CSC.
};
const SecurityCodeCardTypePair kInvalidSecurityCodeCardTypePairs[] = {
    {"32", kGenericCard},             // CSC too short.
    {"323", kAmericanExpressCard},    // CSC too short.
    {"3234", kGenericCard},           // CSC too long.
    {"12345", kAmericanExpressCard},  // CSC too long.
    {"asd", kGenericCard},            // non-numeric CSC.
};
const char* const kValidEmailAddress[] = {
  "user@example",
  "user@example.com",
  "user@subdomain.example.com",
  "user+postfix@example.com",
};
const char* const kInvalidEmailAddress[] = {
  "user",
  "foo.com",
  "user@",
  "user@=example.com"
};
const char* const kUnplausibleCreditCardExpirationYears[] = {
    "2009", "2134", "1111", "abcd", "2101"};
const char* const kPlausibleCreditCardExpirationYears[] = {"2018", "2099",
                                                           "2010", "2050"};
const char* const kUnplausibleCreditCardCVCNumbers[] = {"abc", "21", "11111",
                                                        "21a1"};
const char* const kPlausibleCreditCardCVCNumbers[] = {"1234", "2099", "111",
                                                      "982"};
}  // namespace

TEST(AutofillValidation, IsValidCreditCardNumber) {
  for (const char* valid_number : kValidNumbers) {
    SCOPED_TRACE(valid_number);
    EXPECT_TRUE(IsValidCreditCardNumber(ASCIIToUTF16(valid_number)));
  }
  for (const char* invalid_number : kInvalidNumbers) {
    SCOPED_TRACE(invalid_number);
    EXPECT_FALSE(IsValidCreditCardNumber(ASCIIToUTF16(invalid_number)));
  }
}

// Tests the plausibility of supplied credit card expiration years.
TEST(AutofillValidation, IsPlausibleCreditCardExparationYear) {
  for (const char* plausible_year : kPlausibleCreditCardExpirationYears) {
    EXPECT_TRUE(
        IsPlausible4DigitExpirationYear(base::ASCIIToUTF16(plausible_year)))
        << plausible_year;
  }

  for (const char* unplausible_year : kUnplausibleCreditCardExpirationYears) {
    EXPECT_FALSE(
        IsPlausible4DigitExpirationYear(base::ASCIIToUTF16(unplausible_year)))
        << unplausible_year;
  }
}

// Test the plausibility of supplied CVC numbers.
TEST(AutofillValidation, IsPlausibleCreditCardCVCNumber) {
  for (const char* plausible_cvc : kPlausibleCreditCardCVCNumbers) {
    EXPECT_TRUE(
        IsPlausibleCreditCardCVCNumber(base::ASCIIToUTF16(plausible_cvc)))
        << plausible_cvc;
  }

  for (const char* unplausible_cvc : kUnplausibleCreditCardCVCNumbers) {
    EXPECT_FALSE(
        IsPlausibleCreditCardCVCNumber(base::ASCIIToUTF16(unplausible_cvc)))
        << unplausible_cvc;
  }
}

TEST(AutofillValidation, IsValidCreditCardIntExpirationDate) {
  base::Time now;
  ASSERT_TRUE(base::Time::FromString(kCurrentDate, &now));

  for (const IntExpirationDate& data : kValidCreditCardIntExpirationDate) {
    SCOPED_TRACE(data.year);
    SCOPED_TRACE(data.month);
    EXPECT_TRUE(IsValidCreditCardExpirationDate(data.year, data.month, now));
  }
  for (const IntExpirationDate& data : kInvalidCreditCardIntExpirationDate) {
    SCOPED_TRACE(data.year);
    SCOPED_TRACE(data.month);
    EXPECT_TRUE(!IsValidCreditCardExpirationDate(data.year, data.month, now));
  }
}

TEST(AutofillValidation, IsValidCreditCardSecurityCode) {
  for (const auto data : kValidSecurityCodeCardTypePairs) {
    SCOPED_TRACE(data.security_code);
    SCOPED_TRACE(data.card_type);
    EXPECT_TRUE(IsValidCreditCardSecurityCode(ASCIIToUTF16(data.security_code),
                                              data.card_type));
  }
  for (const auto data : kInvalidSecurityCodeCardTypePairs) {
    SCOPED_TRACE(data.security_code);
    SCOPED_TRACE(data.card_type);
    EXPECT_FALSE(IsValidCreditCardSecurityCode(ASCIIToUTF16(data.security_code),
                                               data.card_type));
  }
}

TEST(AutofillValidation, IsValidEmailAddress) {
  for (const char* valid_email : kValidEmailAddress) {
    SCOPED_TRACE(valid_email);
    EXPECT_TRUE(IsValidEmailAddress(ASCIIToUTF16(valid_email)));
  }
  for (const char* invalid_email : kInvalidEmailAddress) {
    SCOPED_TRACE(invalid_email);
    EXPECT_FALSE(IsValidEmailAddress(ASCIIToUTF16(invalid_email)));
  }
}

struct ValidationCase {
  ValidationCase(const char* value,
                 ServerFieldType field_type,
                 bool expected_valid,
                 int expected_error_id)
      : value(value),
        field_type(field_type),
        expected_valid(expected_valid),
        expected_error_id(expected_error_id) {}
  ~ValidationCase() {}

  const char* const value;
  const ServerFieldType field_type;
  const bool expected_valid;
  const int expected_error_id;
};

class AutofillTypeValidationTest
    : public testing::TestWithParam<ValidationCase> {};

TEST_P(AutofillTypeValidationTest, IsValidForType) {
  base::string16 error_message;
  EXPECT_EQ(GetParam().expected_valid,
            IsValidForType(ASCIIToUTF16(GetParam().value),
                           GetParam().field_type, &error_message))
      << "Failed to validate " << GetParam().value << " (type "
      << GetParam().field_type << ")";
  if (!GetParam().expected_valid) {
    EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().expected_error_id),
              error_message);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardExpDate,
    AutofillTypeValidationTest,
    testing::Values(
        ValidationCase("05/2087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),
        ValidationCase("05-2087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),
        ValidationCase("052087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),
        ValidationCase("05|2087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),

        ValidationCase("05/2012",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        ValidationCase("MM/2012",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase("05/12",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase("05/45",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase("05/1987",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),

        ValidationCase("05/87", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase("05-87", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase("0587", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase("05|87", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase("05/1987",
                       CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase("05/12",
                       CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED)));

INSTANTIATE_TEST_SUITE_P(
    CreditCardMonth,
    AutofillTypeValidationTest,
    testing::Values(
        ValidationCase("01", CREDIT_CARD_EXP_MONTH, true, 0),
        ValidationCase("1", CREDIT_CARD_EXP_MONTH, true, 0),
        ValidationCase("12", CREDIT_CARD_EXP_MONTH, true, 0),
        ValidationCase(
            "0",
            CREDIT_CARD_EXP_MONTH,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_MONTH),
        ValidationCase(
            "-1",
            CREDIT_CARD_EXP_MONTH,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_MONTH),
        ValidationCase(
            "13",
            CREDIT_CARD_EXP_MONTH,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_MONTH)));

INSTANTIATE_TEST_SUITE_P(
    CreditCardYear,
    AutofillTypeValidationTest,
    testing::Values(
        /* 2-digit year */
        ValidationCase("87", CREDIT_CARD_EXP_2_DIGIT_YEAR, true, 0),
        // These are considered expired in the context of this millenium.
        ValidationCase("02",
                       CREDIT_CARD_EXP_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        ValidationCase("15",
                       CREDIT_CARD_EXP_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        // Invalid formats.
        ValidationCase(
            "1",
            CREDIT_CARD_EXP_2_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            "123",
            CREDIT_CARD_EXP_2_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            "2087",
            CREDIT_CARD_EXP_2_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),

        /* 4-digit year */
        ValidationCase("2087", CREDIT_CARD_EXP_4_DIGIT_YEAR, true, 0),
        // Expired.
        ValidationCase("2000",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        ValidationCase("2015",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        // Invalid formats.
        ValidationCase(
            "00",
            CREDIT_CARD_EXP_4_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            "123",
            CREDIT_CARD_EXP_4_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            "87",
            CREDIT_CARD_EXP_4_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR)));

struct CCNumberCase {
  CCNumberCase(const char* value,
               const std::set<std::string> supported_basic_card_networks,
               bool expected_valid,
               int expected_error_id)
      : value(value),
        supported_basic_card_networks(supported_basic_card_networks),
        expected_valid(expected_valid),
        expected_error_id(expected_error_id) {}
  ~CCNumberCase() {}

  const char* const value;
  const std::set<std::string> supported_basic_card_networks;
  const bool expected_valid;
  const int expected_error_id;
};

class AutofillCCNumberValidationTest
    : public testing::TestWithParam<CCNumberCase> {};

TEST_P(AutofillCCNumberValidationTest, IsValidCreditCardNumber) {
  base::string16 error_message;
  EXPECT_EQ(GetParam().expected_valid,
            IsValidCreditCardNumberForBasicCardNetworks(
                ASCIIToUTF16(GetParam().value),
                GetParam().supported_basic_card_networks, &error_message))
      << "Failed to validate CC number " << GetParam().value;
  if (!GetParam().expected_valid) {
    EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().expected_error_id),
              error_message);
  }
}

static const std::set<std::string> kAllBasicCardNetworks{
    "amex",       "discover", "diners",   "elo", "jcb",
    "mastercard", "mir",      "unionpay", "visa"};

INSTANTIATE_TEST_SUITE_P(
    CreditCardNumber,
    AutofillCCNumberValidationTest,
    testing::Values(
        CCNumberCase(kValidNumbers[0], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[1], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[2], kAllBasicCardNetworks, true, 0),
        // Generic card not supported.
        CCNumberCase(kValidNumbers[3],
                     kAllBasicCardNetworks,
                     false,
                     IDS_PAYMENTS_VALIDATION_UNSUPPORTED_CREDIT_CARD_TYPE),

        CCNumberCase(kValidNumbers[4], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[5], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[6], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[7], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[8], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[9], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[10], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[11], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[12], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[13], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[14], kAllBasicCardNetworks, true, 0),
        // Generic cards not supported.
        CCNumberCase(kValidNumbers[15],
                     kAllBasicCardNetworks,
                     false,
                     IDS_PAYMENTS_VALIDATION_UNSUPPORTED_CREDIT_CARD_TYPE),
        CCNumberCase(kValidNumbers[16],
                     kAllBasicCardNetworks,
                     false,
                     IDS_PAYMENTS_VALIDATION_UNSUPPORTED_CREDIT_CARD_TYPE),

        CCNumberCase(kValidNumbers[17], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[18], kAllBasicCardNetworks, true, 0),
        CCNumberCase(kValidNumbers[19], kAllBasicCardNetworks, true, 0),

        CCNumberCase(kInvalidNumbers[0],
                     kAllBasicCardNetworks,
                     false,
                     IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
        CCNumberCase(kInvalidNumbers[1],
                     kAllBasicCardNetworks,
                     false,
                     IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
        CCNumberCase(kInvalidNumbers[2],
                     kAllBasicCardNetworks,
                     false,
                     IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),
        CCNumberCase(kInvalidNumbers[3],
                     kAllBasicCardNetworks,
                     false,
                     IDS_PAYMENTS_CARD_NUMBER_INVALID_VALIDATION_MESSAGE),

        // Valid numbers can still be invalid if the type is not supported.
        CCNumberCase(kValidNumbers[10],  // Mastercard number.
                     {"visa"},
                     false,
                     IDS_PAYMENTS_VALIDATION_UNSUPPORTED_CREDIT_CARD_TYPE),
        CCNumberCase(kValidNumbers[12],  // Visa number.
                     {"jcb", "diners", "unionpay", "mastercard"},
                     false,
                     IDS_PAYMENTS_VALIDATION_UNSUPPORTED_CREDIT_CARD_TYPE)));

struct GetCvcLengthForCardTypeCase {
  GetCvcLengthForCardTypeCase(const char* card_type, size_t expected_length)
      : card_type(card_type), expected_length(expected_length) {}
  ~GetCvcLengthForCardTypeCase() {}

  const char* const card_type;
  const size_t expected_length;
};

class AutofillGetCvcLengthForCardType
    : public testing::TestWithParam<GetCvcLengthForCardTypeCase> {};

TEST_P(AutofillGetCvcLengthForCardType, GetCvcLengthForCardType) {
  EXPECT_EQ(GetParam().expected_length,
            GetCvcLengthForCardType(GetParam().card_type));
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardCvcLength,
    AutofillGetCvcLengthForCardType,
    testing::Values(
        GetCvcLengthForCardTypeCase{kAmericanExpressCard, AMEX_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kDinersCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kDiscoverCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kEloCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kGenericCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kJCBCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kMasterCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kMirCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kUnionPay, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kVisaCard, GENERAL_CVC_LENGTH}));

class AutofillIsUPIVirtualPaymentAddress
    : public testing::TestWithParam<std::string> {};

TEST_P(AutofillIsUPIVirtualPaymentAddress, IsUPIVirtualPaymentAddress_Banks) {
  // Expected format is user@bank
  EXPECT_TRUE(
      IsUPIVirtualPaymentAddress(ASCIIToUTF16("user.name-1@" + GetParam())));

  // Deviations should not match: bank, @bank, user@prefixbank, user@banksuffix,
  // disallowed symbols.
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(ASCIIToUTF16(GetParam())));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(ASCIIToUTF16(GetParam() + "@")));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(ASCIIToUTF16("@" + GetParam())));
  EXPECT_FALSE(
      IsUPIVirtualPaymentAddress(ASCIIToUTF16("user@invalid" + GetParam())));
  EXPECT_FALSE(
      IsUPIVirtualPaymentAddress(ASCIIToUTF16("user@" + GetParam() + ".com")));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(ASCIIToUTF16("~user@" + GetParam())));
}

INSTANTIATE_TEST_SUITE_P(UPIVirtualPaymentAddress,
                         AutofillIsUPIVirtualPaymentAddress,
                         testing::Values("upi",
                                         "allbank",
                                         "andb",
                                         "axisbank",
                                         "barodampay",
                                         "mahb",
                                         "cnrb",
                                         "csbpay",
                                         "dcb",
                                         "federal",
                                         "hdfcbank",
                                         "pockets",
                                         "icici",
                                         "idfcbank",
                                         "indus",
                                         "kbl",
                                         "kaypay",
                                         "pnb",
                                         "sib",
                                         "sbi",
                                         "tjsb",
                                         "uco",
                                         "unionbank",
                                         "united",
                                         "vijb",
                                         "ybl"));

TEST(AutofillValidation, IsUPIVirtualPaymentAddress_Others) {
  EXPECT_TRUE(
      IsUPIVirtualPaymentAddress(ASCIIToUTF16("12345@HDFC0000001.ifsc.npci")));
  EXPECT_TRUE(
      IsUPIVirtualPaymentAddress(ASCIIToUTF16("234567890123@aadhaar.npci")));
  EXPECT_TRUE(
      IsUPIVirtualPaymentAddress(ASCIIToUTF16("9800011111@mobile.npci")));
  EXPECT_TRUE(
      IsUPIVirtualPaymentAddress(ASCIIToUTF16("1234123412341234@rupay.npci")));
}

class AutofillIsInternationalBankAccountNumber
    : public testing::TestWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(InternationalBankAccountNumber,
                         AutofillIsInternationalBankAccountNumber,
                         testing::Values("MT84MALT011000012345MTLCAST001S",
                                         "SC18SSCB11010000000000001497USD",
                                         "MD24AG000225100013104168",
                                         "BH67BMAG00001299123456",
                                         "LI21088100002324013AA",
                                         "NO9386011117947",
                                         "FR1420041010050500013M02606",
                                         "LB62099900000001001901229114"));

TEST_P(AutofillIsInternationalBankAccountNumber,
       IsInternationalBankAccountNumber) {
  EXPECT_TRUE(IsInternationalBankAccountNumber(ASCIIToUTF16(GetParam())))
      << GetParam();
  EXPECT_TRUE(
      IsInternationalBankAccountNumber(ASCIIToUTF16(" " + GetParam() + " ")));
  EXPECT_FALSE(
      IsInternationalBankAccountNumber(ASCIIToUTF16("DE" + GetParam())));
  EXPECT_FALSE(
      IsInternationalBankAccountNumber(ASCIIToUTF16(GetParam() + ".")));
  EXPECT_FALSE(IsInternationalBankAccountNumber(
      ASCIIToUTF16(GetParam() + "0000000000000000000000000000000000000")));
}

}  // namespace autofill
