// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/validation.h"

#include <stddef.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
  const char16_t* security_code;
  const char* card_network;
};

// From
// https://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
const char16_t* const kValidNumbers[] = {
    u"378282246310005",     u"3714 4963 5398 431",  u"3787-3449-3671-000",
    u"5610591081018250",    u"3056 9309 0259 04",   u"3852-0000-0232-37",
    u"6011111111111117",    u"6011 0009 9013 9424", u"3530-1113-3330-0000",
    u"3566002020360505",
    u"5555 5555 5555 4444",  // Mastercard.
    u"5105-1051-0510-5100",
    u"4111111111111111",  // Visa.
    u"4012 8888 8888 1881", u"4222-2222-2222-2",    u"5019717010103742",
    u"6331101999990016",    u"6247130048162403",
    u"4532261615476013542",  // Visa, 19 digits.
    u"6362970000457013",     // Elo
};
const char16_t* const kInvalidNumbers[] = {
    u"4111 1111 112",        /* too short */
    u"41111111111111111115", /* too long */
    u"4111-1111-1111-1110",  /* wrong Luhn checksum */
    u"3056 9309 0259 04aa",  /* non-digit characters */
};
const char kCurrentDate[] = "1 May 2013";
const IntExpirationDate kValidCreditCardIntExpirationDate[] = {
    {2013, 5},   // Valid month in current year.
    {2014, 1},   // Any month in next year.
    {2014, 12},  // Edge condition.
};
const IntExpirationDate kInvalidCreditCardIntExpirationDate[] = {
    {2013, 4},   // Previous month in current year.
    {2012, 12},  // Any month in previous year.
    {2015, 13},  // Not a real month.
    {2015, 0},   // Zero is legal in the CC class but is not a valid date.
};
const SecurityCodeCardTypePair kValidSecurityCodeCardTypePairs[] = {
    {u"323", kGenericCard},           // 3-digit CSC.
    {u"3234", kAmericanExpressCard},  // 4-digit CSC.
};
const SecurityCodeCardTypePair kInvalidSecurityCodeCardTypePairs[] = {
    {u"32", kGenericCard},             // CSC too short.
    {u"323", kAmericanExpressCard},    // CSC too short.
    {u"3234", kGenericCard},           // CSC too long.
    {u"12345", kAmericanExpressCard},  // CSC too long.
    {u"asd", kGenericCard},            // non-numeric CSC.
};
const char16_t* const kValidEmailAddress[] = {
    u"user@example",
    u"user@example.com",
    u"user@subdomain.example.com",
    u"user+postfix@example.com",
};
const char16_t* const kInvalidEmailAddress[] = {u"user", u"foo.com", u"user@",
                                                u"user@=example.com"};
const char16_t* const kUnplausibleCreditCardExpirationYears[] = {
    u"2009", u"2134", u"1111", u"abcd", u"2101"};
const char16_t* const kPlausibleCreditCardExpirationYears[] = {
    u"2018", u"2099", u"2010", u"2050"};
const char16_t* const kUnplausibleCreditCardCVCNumbers[] = {u"abc", u"21",
                                                            u"11111", u"21a1"};
const char16_t* const kPlausibleCreditCardCVCNumbers[] = {u"1234", u"2099",
                                                          u"111", u"982"};
}  // namespace

TEST(AutofillValidation, IsValidCreditCardNumber) {
  for (const char16_t* valid_number : kValidNumbers) {
    SCOPED_TRACE(base::UTF16ToUTF8(valid_number));
    EXPECT_TRUE(IsValidCreditCardNumber(valid_number));
  }
  for (const char16_t* invalid_number : kInvalidNumbers) {
    SCOPED_TRACE(base::UTF16ToUTF8(invalid_number));
    EXPECT_FALSE(IsValidCreditCardNumber(invalid_number));
  }
}

// Tests the plausibility of supplied credit card expiration years.
TEST(AutofillValidation, IsPlausibleCreditCardExparationYear) {
  for (const char16_t* plausible_year : kPlausibleCreditCardExpirationYears) {
    EXPECT_TRUE(IsPlausible4DigitExpirationYear(plausible_year))
        << base::UTF16ToUTF8(plausible_year);
  }

  for (const char16_t* unplausible_year :
       kUnplausibleCreditCardExpirationYears) {
    EXPECT_FALSE(IsPlausible4DigitExpirationYear(unplausible_year))
        << base::UTF16ToUTF8(unplausible_year);
  }
}

// Test the plausibility of supplied CVC numbers.
TEST(AutofillValidation, IsPlausibleCreditCardCVCNumber) {
  for (const char16_t* plausible_cvc : kPlausibleCreditCardCVCNumbers) {
    EXPECT_TRUE(IsPlausibleCreditCardCVCNumber(plausible_cvc))
        << base::UTF16ToUTF8(plausible_cvc);
  }

  for (const char16_t* unplausible_cvc : kUnplausibleCreditCardCVCNumbers) {
    EXPECT_FALSE(IsPlausibleCreditCardCVCNumber(unplausible_cvc))
        << base::UTF16ToUTF8(unplausible_cvc);
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
    SCOPED_TRACE(base::UTF16ToUTF8(data.security_code));
    SCOPED_TRACE(data.card_network);
    EXPECT_TRUE(
        IsValidCreditCardSecurityCode(data.security_code, data.card_network));
  }
  for (const auto data : kInvalidSecurityCodeCardTypePairs) {
    SCOPED_TRACE(base::UTF16ToUTF8(data.security_code));
    SCOPED_TRACE(data.card_network);
    EXPECT_FALSE(
        IsValidCreditCardSecurityCode(data.security_code, data.card_network));
  }
}

TEST(AutofillValidation, IsValidCreditCardSecurityCode_BackOfAmexCvc) {
  // For back of card American Express, expect a three digit CVC.
  SCOPED_TRACE(base::UTF16ToUTF8(u"123"));
  SCOPED_TRACE(kAmericanExpressCard);
  EXPECT_TRUE(IsValidCreditCardSecurityCode(u"123", kAmericanExpressCard,
                                            CvcType::kBackOfAmexCvc));
}

TEST(AutofillValidation, IsInvalidCreditCardSecurityCode_BackOfAmexCvc) {
  // For back of card American Express, expect a three digit CVC.
  SCOPED_TRACE(base::UTF16ToUTF8(u"1234"));
  SCOPED_TRACE(kAmericanExpressCard);
  EXPECT_FALSE(IsValidCreditCardSecurityCode(u"1234", kAmericanExpressCard,
                                             CvcType::kBackOfAmexCvc));
}

TEST(AutofillValidation, IsValidEmailAddress) {
  for (const char16_t* valid_email : kValidEmailAddress) {
    SCOPED_TRACE(base::UTF16ToUTF8(valid_email));
    EXPECT_TRUE(IsValidEmailAddress(valid_email));
  }
  for (const char16_t* invalid_email : kInvalidEmailAddress) {
    SCOPED_TRACE(base::UTF16ToUTF8(invalid_email));
    EXPECT_FALSE(IsValidEmailAddress(invalid_email));
  }
}

struct ValidationCase {
  ValidationCase(const char16_t* value,
                 ServerFieldType field_type,
                 bool expected_valid,
                 int expected_error_id)
      : value(value),
        field_type(field_type),
        expected_valid(expected_valid),
        expected_error_id(expected_error_id) {}
  ~ValidationCase() {}

  const char16_t* const value;
  const ServerFieldType field_type;
  const bool expected_valid;
  const int expected_error_id;
};

class AutofillTypeValidationTest
    : public testing::TestWithParam<ValidationCase> {};

TEST_P(AutofillTypeValidationTest, IsValidForType) {
  std::u16string error_message;
  EXPECT_EQ(
      GetParam().expected_valid,
      IsValidForType(GetParam().value, GetParam().field_type, &error_message))
      << "Failed to validate " << base::UTF16ToUTF8(GetParam().value)
      << " (type " << GetParam().field_type << ")";
  if (!GetParam().expected_valid) {
    EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().expected_error_id),
              error_message);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardExpDate,
    AutofillTypeValidationTest,
    testing::Values(
        ValidationCase(u"05/2087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),
        ValidationCase(u"05-2087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),
        ValidationCase(u"052087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),
        ValidationCase(u"05|2087", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, true, 0),

        ValidationCase(u"05/2012",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        ValidationCase(u"MM/2012",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase(u"05/12",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase(u"05/45",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase(u"05/1987",
                       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),

        ValidationCase(u"05/87", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase(u"05-87", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase(u"0587", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase(u"05|87", CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, true, 0),
        ValidationCase(u"05/1987",
                       CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_CARD_EXPIRATION_INVALID_VALIDATION_MESSAGE),
        ValidationCase(u"05/12",
                       CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED)));

INSTANTIATE_TEST_SUITE_P(
    CreditCardMonth,
    AutofillTypeValidationTest,
    testing::Values(
        ValidationCase(u"01", CREDIT_CARD_EXP_MONTH, true, 0),
        ValidationCase(u"1", CREDIT_CARD_EXP_MONTH, true, 0),
        ValidationCase(u"12", CREDIT_CARD_EXP_MONTH, true, 0),
        ValidationCase(
            u"0",
            CREDIT_CARD_EXP_MONTH,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_MONTH),
        ValidationCase(
            u"-1",
            CREDIT_CARD_EXP_MONTH,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_MONTH),
        ValidationCase(
            u"13",
            CREDIT_CARD_EXP_MONTH,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_MONTH)));

INSTANTIATE_TEST_SUITE_P(
    CreditCardYear,
    AutofillTypeValidationTest,
    testing::Values(
        /* 2-digit year */
        ValidationCase(u"87", CREDIT_CARD_EXP_2_DIGIT_YEAR, true, 0),
        // These are considered expired in the context of this millennium.
        ValidationCase(u"02",
                       CREDIT_CARD_EXP_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        ValidationCase(u"15",
                       CREDIT_CARD_EXP_2_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        // Invalid formats.
        ValidationCase(
            u"1",
            CREDIT_CARD_EXP_2_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            u"123",
            CREDIT_CARD_EXP_2_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            u"2087",
            CREDIT_CARD_EXP_2_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),

        /* 4-digit year */
        ValidationCase(u"2087", CREDIT_CARD_EXP_4_DIGIT_YEAR, true, 0),
        // Expired.
        ValidationCase(u"2000",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        ValidationCase(u"2015",
                       CREDIT_CARD_EXP_4_DIGIT_YEAR,
                       false,
                       IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRED),
        // Invalid formats.
        ValidationCase(
            u"00",
            CREDIT_CARD_EXP_4_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            u"123",
            CREDIT_CARD_EXP_4_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR),
        ValidationCase(
            u"87",
            CREDIT_CARD_EXP_4_DIGIT_YEAR,
            false,
            IDS_PAYMENTS_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_YEAR)));

struct CCNumberCase {
  CCNumberCase(const char16_t* value,
               const std::set<std::string> supported_basic_card_networks,
               bool expected_valid,
               int expected_error_id)
      : value(value),
        supported_basic_card_networks(supported_basic_card_networks),
        expected_valid(expected_valid),
        expected_error_id(expected_error_id) {}
  ~CCNumberCase() {}

  const char16_t* const value;
  const std::set<std::string> supported_basic_card_networks;
  const bool expected_valid;
  const int expected_error_id;
};

class AutofillCCNumberValidationTest
    : public testing::TestWithParam<CCNumberCase> {};

TEST_P(AutofillCCNumberValidationTest, IsValidCreditCardNumber) {
  std::u16string error_message;
  EXPECT_EQ(GetParam().expected_valid,
            IsValidCreditCardNumberForBasicCardNetworks(
                GetParam().value, GetParam().supported_basic_card_networks,
                &error_message))
      << "Failed to validate CC number " << base::UTF16ToUTF8(GetParam().value);
  if (!GetParam().expected_valid) {
    EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().expected_error_id),
              error_message);
  }
}

static const std::set<std::string> kAllBasicCardNetworks{
    "amex",       "discover", "diners", "elo",      "jcb",
    "mastercard", "mir",      "troy",   "unionpay", "visa"};

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
  GetCvcLengthForCardTypeCase(const char* card_network,
                              size_t expected_length,
                              CvcType cvc_type = CvcType::kRegularCvc)
      : card_network(card_network),
        expected_length(expected_length),
        cvc_type(cvc_type) {}
  ~GetCvcLengthForCardTypeCase() = default;

  const char* const card_network;
  const size_t expected_length;
  CvcType cvc_type;
};

class AutofillGetCvcLengthForCardType
    : public testing::TestWithParam<GetCvcLengthForCardTypeCase> {};

TEST_P(AutofillGetCvcLengthForCardType, GetCvcLengthForCardNetwork) {
  EXPECT_EQ(
      GetParam().expected_length,
      GetCvcLengthForCardNetwork(GetParam().card_network, GetParam().cvc_type));
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
        GetCvcLengthForCardTypeCase{kTroyCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kUnionPay, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kVisaCard, GENERAL_CVC_LENGTH},
        GetCvcLengthForCardTypeCase{kAmericanExpressCard, GENERAL_CVC_LENGTH,
                                    CvcType::kBackOfAmexCvc}));

class AutofillIsUPIVirtualPaymentAddress
    : public testing::TestWithParam<std::u16string> {};

TEST_P(AutofillIsUPIVirtualPaymentAddress, IsUPIVirtualPaymentAddress_Banks) {
  // Expected format is user@bank
  EXPECT_TRUE(IsUPIVirtualPaymentAddress(u"user.name-1@" + GetParam()));

  // Deviations should not match: bank, @bank, user@prefixbank, user@banksuffix,
  // disallowed symbols.
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(GetParam()));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(GetParam() + u"@"));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(u"@" + GetParam()));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(u"user@invalid" + GetParam()));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(u"user@" + GetParam() + u".com"));
  EXPECT_FALSE(IsUPIVirtualPaymentAddress(u"~user@" + GetParam()));
}

INSTANTIATE_TEST_SUITE_P(UPIVirtualPaymentAddress,
                         AutofillIsUPIVirtualPaymentAddress,
                         testing::Values(u"upi",
                                         u"allbank",
                                         u"andb",
                                         u"axisbank",
                                         u"barodampay",
                                         u"mahb",
                                         u"cnrb",
                                         u"csbpay",
                                         u"dcb",
                                         u"federal",
                                         u"hdfcbank",
                                         u"pockets",
                                         u"icici",
                                         u"idfcbank",
                                         u"indus",
                                         u"kbl",
                                         u"kaypay",
                                         u"pnb",
                                         u"sib",
                                         u"sbi",
                                         u"tjsb",
                                         u"uco",
                                         u"unionbank",
                                         u"united",
                                         u"vijb",
                                         u"ybl"));

TEST(AutofillValidation, IsUPIVirtualPaymentAddress_Others) {
  EXPECT_TRUE(IsUPIVirtualPaymentAddress(u"12345@HDFC0000001.ifsc.npci"));
  EXPECT_TRUE(IsUPIVirtualPaymentAddress(u"234567890123@aadhaar.npci"));
  EXPECT_TRUE(IsUPIVirtualPaymentAddress(u"9800011111@mobile.npci"));
  EXPECT_TRUE(IsUPIVirtualPaymentAddress(u"1234123412341234@rupay.npci"));
}

class AutofillIsInternationalBankAccountNumber
    : public testing::TestWithParam<std::u16string> {};

INSTANTIATE_TEST_SUITE_P(InternationalBankAccountNumber,
                         AutofillIsInternationalBankAccountNumber,
                         testing::Values(u"MT84MALT011000012345MTLCAST001S",
                                         u"SC18SSCB11010000000000001497USD",
                                         u"MD24AG000225100013104168",
                                         u"BH67BMAG00001299123456",
                                         u"LI21088100002324013AA",
                                         u"NO9386011117947",
                                         u"FR1420041010050500013M02606",
                                         u"LB62099900000001001901229114"));

TEST_P(AutofillIsInternationalBankAccountNumber,
       IsInternationalBankAccountNumber) {
  EXPECT_TRUE(IsInternationalBankAccountNumber(GetParam()))
      << base::StringPiece16(GetParam());
  EXPECT_TRUE(IsInternationalBankAccountNumber(u" " + GetParam() + u" "));
  EXPECT_FALSE(IsInternationalBankAccountNumber(u"DE" + GetParam()));
  EXPECT_FALSE(IsInternationalBankAccountNumber(GetParam() + u"."));
  EXPECT_FALSE(IsInternationalBankAccountNumber(
      GetParam() + u"0000000000000000000000000000000000000"));
}

}  // namespace autofill
