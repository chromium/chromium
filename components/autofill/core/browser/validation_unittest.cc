// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/validation.h"

#include <stddef.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  EXPECT_TRUE(IsInternationalBankAccountNumber(GetParam())) << GetParam();
  EXPECT_TRUE(IsInternationalBankAccountNumber(u" " + GetParam() + u" "));
  EXPECT_FALSE(IsInternationalBankAccountNumber(u"DE" + GetParam()));
  EXPECT_FALSE(IsInternationalBankAccountNumber(GetParam() + u"."));
  EXPECT_FALSE(IsInternationalBankAccountNumber(
      GetParam() + u"0000000000000000000000000000000000000"));
}

}  // namespace
}  // namespace autofill
