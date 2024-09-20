// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card.h"

#include <stddef.h>

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/payments_metadata.h"
#include "components/autofill/core/browser/data_model/test_autofill_data_model.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {
namespace {

const CreditCard::RecordType LOCAL_CARD = CreditCard::RecordType::kLocalCard;
const CreditCard::RecordType MASKED_SERVER_CARD =
    CreditCard::RecordType::kMaskedServerCard;
const CreditCard::RecordType FULL_SERVER_CARD =
    CreditCard::RecordType::kFullServerCard;

// From
// https://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
const char* const kValidNumbers[] = {
    "378282246310005",     "3714 4963 5398 431",  "3787-3449-3671-000",
    "5610591081018250",    "3056 9309 0259 04",   "3852-0000-0232-37",
    "6011111111111117",    "6011 0009 9013 9424", "3530-1113-3330-0000",
    "3566002020360505",    "5555 5555 5555 4444", "5105-1051-0510-5100",
    "4111111111111111",    "4012 8888 8888 1881", "4222-2222-2222-2",
    "5019717010103742",    "6331101999990016",    "6247130048162403",
    "4532261615476013542", "6362970000457013",
};

const char* const kInvalidNumbers[] = {
    "4111 1111 112",        /* too short */
    "41111111111111111115", /* too long */
    "4111-1111-1111-1110",  /* wrong Luhn checksum */
    "3056 9309 0259 04aa",  /* non-digit characters */
};

const char* const kValidNicknames[] = {
    "Grocery Card",
    "Two percent Cashback",
    "Mastercard \xF0\x9F\x92\xB3", /* Nickname with UTF-8 hex encoded emoji */
    "\u0634\u063a\u0645\u0688",    /* arbitrary Arabic script in unicode */
    "\u0434\u0444\u0431\u044A",    /* arbitrary Cyrillic script in unicode */
};

const char* const kInvalidNicknames[] = {
    "Nickname length exceeds 25 characters", /* too long */
    "\t\r\n  ",                              /* empty after SetNickname */
    "CVC: 123",                              /* contains digits */
    "1% cashback",                           /* contains digits */
};

const char* const kEmptyNickname = "";

// Time moves on. Today is yesterday's tomorrow. Tests don't like time moving
// on, in particular if Credit Card expiration is compared to local time.
// Use this function to generate a year in the future.
std::u16string GetYearInTheFuture() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString16(now.year + 4);
}

TEST(CreditCardTest, GetObfuscatedStringForCardDigits) {
  const std::u16string digits = u"1235";
  const std::u16string expected =
      std::u16string() + base::i18n::kLeftToRightEmbeddingMark +
      CreditCard::GetMidlineEllipsisDots(4) + digits +
      base::i18n::kPopDirectionalFormatting;
  EXPECT_EQ(expected, CreditCard::GetObfuscatedStringForCardDigits(
                          /*obfuscation_length=*/4, digits));
}

// Tests credit card summary string generation.  This test simulates a variety
// of different possible summary strings.  Variations occur based on the
// existence of credit card number, month, and year fields.
TEST(CreditCardTest, LabelSummary) {
  std::u16string valid_nickname = u"My Visa Card";

  // Case 0: empty credit card.
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  EXPECT_EQ(std::u16string(), credit_card0.Label());

  // Case 00: Empty credit card with empty strings.
  CreditCard credit_card00(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card00, "John Dillinger", "", "", "", "");
  EXPECT_EQ(std::u16string(u"John Dillinger"), credit_card00.Label());

  // Case 1: No credit card number.
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2010",
                          "1");
  EXPECT_EQ(std::u16string(u"John Dillinger"), credit_card1.Label());

  // Case 1.1: No credit card number, but has nickname.
  CreditCard credit_card11(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card11, "John Dillinger", "", "01", "2010",
                          "1");
  credit_card11.SetNickname(valid_nickname);
  EXPECT_EQ(valid_nickname, credit_card11.Label());

  // Case 2: No month.
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "5105 1051 0510 5100", "", "2010", "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4) +
                        ", John Dillinger"),
            credit_card2.Label());

  // Case 3: No year.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card3, "John Dillinger",
                          "5105 1051 0510 5100", "01", "", "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4) +
                        ", John Dillinger"),
            credit_card3.Label());

  // Case 4: Have everything except nickname.
  CreditCard credit_card4(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card4, "John Dillinger",
                          "5105 1051 0510 5100", "01", "2010", "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4) +
                        ", John Dillinger"),
            credit_card4.Label());

  // Case 5: Very long credit card
  CreditCard credit_card5(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(
      &credit_card5, "John Dillinger",
      "0123456789 0123456789 0123456789 5105 1051 0510 5100", "01", "2010",
      "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Card  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4) +
                        ", John Dillinger"),
            credit_card5.Label());

  // Case 6: Have everything including nickname.
  CreditCard credit_card6(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card6, "John Dillinger",
                          "5105 1051 0510 5100", "01", "2010", "1");
  credit_card6.SetNickname(valid_nickname);
  EXPECT_EQ(
      valid_nickname + UTF8ToUTF16(std::string("  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("5100", 4) +
                                   ", John Dillinger"),
      credit_card6.Label());
}

TEST(CreditCardTest, NetworkAndLastFourDigits) {
  std::u16string valid_nickname = u"My Visa Card";

  // Case 0: empty credit card.
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  EXPECT_EQ(ASCIIToUTF16(std::string("Card")),
            credit_card0.NetworkAndLastFourDigits());

  // Case 00: Empty credit card with empty strings.
  CreditCard credit_card00(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card00, "John Dillinger", "", "", "", "");
  EXPECT_EQ(ASCIIToUTF16(std::string("Card")),
            credit_card00.NetworkAndLastFourDigits());

  // Case 1: No credit card number.
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2010",
                          "1");
  EXPECT_EQ(ASCIIToUTF16(std::string("Card")),
            credit_card1.NetworkAndLastFourDigits());

  // Case 1.1: No credit card number, but has nickname.
  CreditCard credit_card11(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card11, "John Dillinger", "", "01", "2010",
                          "1");
  credit_card11.SetNickname(valid_nickname);
  EXPECT_EQ(ASCIIToUTF16(std::string("Card")),
            credit_card11.NetworkAndLastFourDigits());

  // Case 2: No month.
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "5105 1051 0510 5100", "", "2010", "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card2.NetworkAndLastFourDigits());

  // Case 3: No year.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card3, "John Dillinger",
                          "5105 1051 0510 5100", "01", "", "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card3.NetworkAndLastFourDigits());

  // Case 4: Have everything except nickname.
  CreditCard credit_card4(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card4, "John Dillinger",
                          "5105 1051 0510 5100", "01", "2010", "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card4.NetworkAndLastFourDigits());

  // Case 5: Very long credit card
  CreditCard credit_card5(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(
      &credit_card5, "John Dillinger",
      "0123456789 0123456789 0123456789 5105 1051 0510 5100", "01", "2010",
      "1");
  EXPECT_EQ(UTF8ToUTF16(std::string("Card  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card5.NetworkAndLastFourDigits());

  // Case 6: Have everything including nickname.
  CreditCard credit_card6(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card6, "John Dillinger",
                          "5105 1051 0510 5100", "01", "2010", "1");
  credit_card6.SetNickname(valid_nickname);
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card6.NetworkAndLastFourDigits());
}

TEST(CreditCardTest, NicknameAndLastFourDigitsStrings) {
  std::u16string valid_nickname = u"My Visa Card";

  // Case 1: No credit card number but has nickname. Only return nickname.
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2020",
                          "1");
  credit_card1.SetNickname(valid_nickname);
  EXPECT_EQ(valid_nickname, credit_card1.NicknameAndLastFourDigitsForTesting());

  // Case 2: Have everything.
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "5105 1051 0510 5100", "01", "2020", "1");
  credit_card2.SetNickname(valid_nickname);
  EXPECT_EQ(
      valid_nickname + UTF8ToUTF16(std::string("  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
      credit_card2.NicknameAndLastFourDigitsForTesting());
}

// Test that card identifier string falls back to issuer network when both
// nickname and product description are unavailable.
TEST(CreditCardTest,
     CardIdentifierStringsForAutofillDisplay_NoNicknameNoProductDescription) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardProductName);

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "5105 1051 0510 5100" /* Mastercard */, "01", "2020",
                          "1");
  EXPECT_FALSE(credit_card.HasNonEmptyValidNickname());
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card.CardNameAndLastFourDigits());
}

// Test that card identifier string falls back to issuer network when nickname
// is invalid and product description is unavailable.
TEST(
    CreditCardTest,
    CardIdentifierStringsForAutofillDisplay_InvalidNicknameNoProductDescription) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardProductName);

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "5105 1051 0510 5100" /* Mastercard */, "01", "2020",
                          "1");
  credit_card.SetNickname(u"Nickname length exceeds 25 characters");
  EXPECT_FALSE(credit_card.HasNonEmptyValidNickname());
  EXPECT_EQ(UTF8ToUTF16(std::string("Mastercard  ") +
                        test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card.CardNameAndLastFourDigits());
}

// Test that card identifier string falls back to product description when
// nickname is unavailable.
TEST(CreditCardTest,
     CardIdentifierStringsForAutofillDisplay_NoNicknameWithProductDescription) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardProductName);

  std::u16string product_description = u"ABC bank XYZ card";

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "5105 1051 0510 5100" /* Mastercard */, "01", "2020",
                          "1");
  credit_card.set_product_description(product_description);
  EXPECT_FALSE(credit_card.HasNonEmptyValidNickname());
  EXPECT_EQ(product_description +
                UTF8ToUTF16(std::string("  ") +
                            test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card.CardNameAndLastFourDigits());
}

// Test that card identifier string falls back to product description when
// nickname is invalid.
TEST(
    CreditCardTest,
    CardIdentifierStringsForAutofillDisplay_InvalidNicknameWithProductDescription) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardProductName);

  std::u16string product_description = u"ABC bank XYZ card";

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "5105 1051 0510 5100" /* Mastercard */, "01", "2020",
                          "1");
  credit_card.SetNickname(u"Nickname length exceeds 25 characters");
  credit_card.set_product_description(product_description);
  EXPECT_FALSE(credit_card.HasNonEmptyValidNickname());
  EXPECT_EQ(product_description +
                UTF8ToUTF16(std::string("  ") +
                            test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card.CardNameAndLastFourDigits());
}

// Test that card identifier string shows nickname when it is valid.
TEST(CreditCardTest,
     CardIdentifierStringsForAutofillDisplay_WithValidNickname) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardProductName);

  std::u16string valid_nickname = u"My Visa Card";

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "5105 1051 0510 5100" /* Mastercard */, "01", "2020",
                          "1");
  credit_card.SetNickname(valid_nickname);
  credit_card.set_product_description(u"ABC bank XYZ card");
  EXPECT_TRUE(credit_card.HasNonEmptyValidNickname());
  EXPECT_EQ(
      valid_nickname + UTF8ToUTF16(std::string("  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
      credit_card.CardNameAndLastFourDigits());
}

// Test that customized nickname takes precedence over credit card's nickname.
TEST(CreditCardTest,
     CardIdentifierStringsForAutofillDisplay_WithCustomizedNickname) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardProductName);

  std::u16string customized_nickname = u"My grocery shopping Visa card";

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "5105 1051 0510 5100" /* Mastercard */, "01", "2020",
                          "1");
  credit_card.SetNickname(u"My Visa Card");
  credit_card.set_product_description(u"ABC bank XYZ card");
  EXPECT_TRUE(credit_card.HasNonEmptyValidNickname());
  EXPECT_EQ(customized_nickname +
                UTF8ToUTF16(std::string("  ") +
                            test::ObfuscatedCardDigitsAsUTF8("5100", 4)),
            credit_card.CardNameAndLastFourDigits(customized_nickname));
}

// Test that the card number is formatted as per the obfuscation length.
TEST(CreditCardTest,
     CardIdentifierStringsForAutofillDisplay_WithObfuscationLength) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableCardProductName);

  int obfuscation_length = 2;

  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "5105 1051 0510 5100" /* Mastercard */, "01", "2020",
                          "1");
  EXPECT_EQ(
      UTF8ToUTF16(std::string("Mastercard  ") +
                  test::ObfuscatedCardDigitsAsUTF8("5100", obfuscation_length)),
      credit_card.CardNameAndLastFourDigits(u"", obfuscation_length));
}

TEST(CreditCardTest, AssignmentOperator) {
  CreditCard a(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               test::kEmptyOrigin);
  test::SetCreditCardInfo(&a, "John Dillinger", "123456789012", "01", "2010",
                          "1");

  // Result of assignment should be logically equal to the original profile.
  CreditCard b(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               test::kEmptyOrigin);
  b = a;
  EXPECT_EQ(a, b);

  // Assignment to self should not change the profile value.
  a = *&a;  // The *& defeats Clang's -Wself-assign warning.
  EXPECT_EQ(a, b);
}

TEST(CreditCardTest, GetMetadata) {
  CreditCard local_card = test::GetCreditCard();
  local_card.set_use_count(2);
  local_card.set_use_date(base::Time::FromSecondsSinceUnixEpoch(25));
  local_card.set_billing_address_id("123");
  PaymentsMetadata local_metadata = local_card.GetMetadata();
  EXPECT_EQ(local_card.guid(), local_metadata.id);
  EXPECT_EQ(local_card.billing_address_id(), local_metadata.billing_address_id);
  EXPECT_EQ(local_card.use_count(), local_metadata.use_count);
  EXPECT_EQ(local_card.use_date(), local_metadata.use_date);

  CreditCard masked_card = test::GetMaskedServerCard();
  masked_card.set_use_count(4);
  masked_card.set_use_date(base::Time::FromSecondsSinceUnixEpoch(50));
  masked_card.set_billing_address_id("abc");
  PaymentsMetadata masked_metadata = masked_card.GetMetadata();
  EXPECT_EQ(masked_card.server_id(), masked_metadata.id);
  EXPECT_EQ(masked_card.billing_address_id(),
            masked_metadata.billing_address_id);
  EXPECT_EQ(masked_card.use_count(), masked_metadata.use_count);
  EXPECT_EQ(masked_card.use_date(), masked_metadata.use_date);

  CreditCard full_card = test::GetFullServerCard();
  full_card.set_use_count(6);
  full_card.set_use_date(base::Time::FromSecondsSinceUnixEpoch(100));
  full_card.set_billing_address_id("xyz");
  PaymentsMetadata full_metadata = full_card.GetMetadata();
  EXPECT_EQ(full_card.server_id(), full_metadata.id);
  EXPECT_EQ(full_card.billing_address_id(), full_metadata.billing_address_id);
  EXPECT_EQ(full_card.use_count(), full_metadata.use_count);
  EXPECT_EQ(full_card.use_date(), full_metadata.use_date);
}

TEST(CreditCardTest, SetMetadata_MatchingId) {
  CreditCard local_card = test::GetCreditCard();
  PaymentsMetadata local_metadata;
  local_metadata.id = local_card.guid();
  local_metadata.use_count = 100;
  local_metadata.use_date = base::Time::FromSecondsSinceUnixEpoch(50);
  local_metadata.billing_address_id = "billId1";
  EXPECT_TRUE(local_card.SetMetadata(local_metadata));
  EXPECT_EQ(local_metadata.id, local_card.guid());
  EXPECT_EQ(local_metadata.billing_address_id, local_card.billing_address_id());
  EXPECT_EQ(local_metadata.use_count, local_card.use_count());
  EXPECT_EQ(local_metadata.use_date, local_card.use_date());

  CreditCard masked_card = test::GetMaskedServerCard();
  PaymentsMetadata masked_metadata;
  masked_metadata.id = masked_card.server_id();
  masked_metadata.use_count = 100;
  masked_metadata.use_date = base::Time::FromSecondsSinceUnixEpoch(50);
  masked_metadata.billing_address_id = "billId1";
  EXPECT_TRUE(masked_card.SetMetadata(masked_metadata));
  EXPECT_EQ(masked_metadata.id, masked_card.server_id());
  EXPECT_EQ(masked_metadata.billing_address_id,
            masked_card.billing_address_id());
  EXPECT_EQ(masked_metadata.use_count, masked_card.use_count());
  EXPECT_EQ(masked_metadata.use_date, masked_card.use_date());

  CreditCard full_card = test::GetFullServerCard();
  PaymentsMetadata full_metadata;
  full_metadata.id = full_card.server_id();
  full_metadata.use_count = 100;
  full_metadata.use_date = base::Time::FromSecondsSinceUnixEpoch(50);
  full_metadata.billing_address_id = "billId1";
  EXPECT_TRUE(full_card.SetMetadata(full_metadata));
  EXPECT_EQ(full_metadata.id, full_card.server_id());
  EXPECT_EQ(full_metadata.billing_address_id, full_card.billing_address_id());
  EXPECT_EQ(full_metadata.use_count, full_card.use_count());
  EXPECT_EQ(full_metadata.use_date, full_card.use_date());
}

TEST(CreditCardTest, SetMetadata_NotMatchingId) {
  CreditCard local_card = test::GetCreditCard();
  PaymentsMetadata local_metadata;
  local_metadata.id = "WrongId";
  local_metadata.use_count = 100;
  local_metadata.use_date = base::Time::FromSecondsSinceUnixEpoch(50);
  local_metadata.billing_address_id = "billId1";
  EXPECT_FALSE(local_card.SetMetadata(local_metadata));
  EXPECT_NE(local_metadata.id, local_card.guid());
  EXPECT_NE(local_metadata.billing_address_id, local_card.billing_address_id());
  EXPECT_NE(local_metadata.use_count, local_card.use_count());
  EXPECT_NE(local_metadata.use_date, local_card.use_date());

  CreditCard masked_card = test::GetMaskedServerCard();
  PaymentsMetadata masked_metadata;
  masked_metadata.id = "WrongId";
  masked_metadata.use_count = 100;
  masked_metadata.use_date = base::Time::FromSecondsSinceUnixEpoch(50);
  masked_metadata.billing_address_id = "billId1";
  EXPECT_FALSE(masked_card.SetMetadata(masked_metadata));
  EXPECT_NE(masked_metadata.id, masked_card.server_id());
  EXPECT_NE(masked_metadata.billing_address_id,
            masked_card.billing_address_id());
  EXPECT_NE(masked_metadata.use_count, masked_card.use_count());
  EXPECT_NE(masked_metadata.use_date, masked_card.use_date());

  CreditCard full_card = test::GetFullServerCard();
  PaymentsMetadata full_metadata;
  full_metadata.id = "WrongId";
  full_metadata.use_count = 100;
  full_metadata.use_date = base::Time::FromSecondsSinceUnixEpoch(50);
  full_metadata.billing_address_id = "billId1";
  EXPECT_FALSE(full_card.SetMetadata(full_metadata));
  EXPECT_NE(full_metadata.id, full_card.server_id());
  EXPECT_NE(full_metadata.billing_address_id, full_card.billing_address_id());
  EXPECT_NE(full_metadata.use_count, full_card.use_count());
  EXPECT_NE(full_metadata.use_date, full_card.use_date());
}

// Test that if one of the two compared cards is masked server card,
// `HasSameNumberAs` returns true if the last four are the same. For all the
// other comparing card types (none of them is masked server card),
// `HasSameNumberAs` returns true if the full card number are the same.
TEST(CreditCardTest, HasSameNumberAs) {
  // Creates three types (local card, masked server card and full server card)
  // of credit cards with the same number.
  CreditCard local_card = test::GetCreditCard();
  CreditCard masked_server_card = test::GetMaskedServerCardVisa();
  CreditCard full_server_card = test::GetFullServerCard();

  // Verify that card number is the same for all combinations of card type.
  EXPECT_TRUE(local_card.HasSameNumberAs(local_card));
  EXPECT_TRUE(local_card.HasSameNumberAs(masked_server_card));
  EXPECT_TRUE(local_card.HasSameNumberAs(full_server_card));
  EXPECT_TRUE(masked_server_card.HasSameNumberAs(masked_server_card));
  EXPECT_TRUE(masked_server_card.HasSameNumberAs(full_server_card));
  EXPECT_TRUE(full_server_card.HasSameNumberAs(full_server_card));

  // Update the local card and full server card number to a different number but
  // all the three credit cards are with same last four.
  local_card.SetRawInfo(CREDIT_CARD_NUMBER, u"4111 1111 0006 1111");
  full_server_card.SetRawInfo(CREDIT_CARD_NUMBER, u"4111 1111 2226 1111");

  // Verify that only last 4 is compared if one of the compared cards is a
  // masked server card; for all other types, full card number is compared.
  EXPECT_TRUE(local_card.HasSameNumberAs(masked_server_card));
  EXPECT_FALSE(local_card.HasSameNumberAs(full_server_card));
  EXPECT_TRUE(masked_server_card.HasSameNumberAs(full_server_card));
}

// Test that `HasSameExpirationDateAs` returns true only if two cards have the
// same expiration year and month.
TEST(CreditCardTest, HasSameExpirationDateAs) {
  CreditCard card_1;
  test::SetCreditCardInfo(&card_1, "John Dillinger", "4111 1111 1111 1111",
                          "09", "2017", "1");

  CreditCard card_2;
  // Set the same expiration date as `card_1`.
  test::SetCreditCardInfo(&card_2, "John Dillinger", "4111 1111 1111 1111",
                          "09", "2017", "1");
  EXPECT_TRUE(card_1.HasSameExpirationDateAs(card_2));

  // Set the same month and different year as `card_1`.
  test::SetCreditCardInfo(&card_2, "John Dillinger", "4111 1111 1111 1111",
                          "09", "2018", "1");
  EXPECT_FALSE(card_1.HasSameExpirationDateAs(card_2));

  // Set the same year and different month as `card_1`.
  test::SetCreditCardInfo(&card_2, "John Dillinger", "4111 1111 1111 1111",
                          "01", "2017", "1");
  EXPECT_FALSE(card_1.HasSameExpirationDateAs(card_2));

  // Set the different expiration date as `card_1`.
  test::SetCreditCardInfo(&card_2, "John Dillinger", "4111 1111 1111 1111",
                          "01", "2018", "1");
  EXPECT_FALSE(card_1.HasSameExpirationDateAs(card_2));
}

struct SetExpirationYearFromStringTestCase {
  std::string expiration_year;
  int expected_year;
};

class SetExpirationYearFromStringTest
    : public testing::TestWithParam<SetExpirationYearFromStringTestCase> {};

TEST_P(SetExpirationYearFromStringTest, SetExpirationYearFromString) {
  auto test_case = GetParam();
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "some origin");
  card.SetExpirationYearFromString(ASCIIToUTF16(test_case.expiration_year));

  EXPECT_EQ(test_case.expected_year, card.expiration_year())
      << test_case.expiration_year << " " << test_case.expected_year;
}

INSTANTIATE_TEST_SUITE_P(CreditCardTest,
                         SetExpirationYearFromStringTest,
                         testing::Values(
                             // Valid values.
                             SetExpirationYearFromStringTestCase{"2040", 2040},
                             SetExpirationYearFromStringTestCase{"45", 2045},
                             SetExpirationYearFromStringTestCase{"045", 2045},
                             SetExpirationYearFromStringTestCase{"9", 2009},

                             // Unrecognized year values.
                             SetExpirationYearFromStringTestCase{"052045", 0},
                             SetExpirationYearFromStringTestCase{"123", 0},
                             SetExpirationYearFromStringTestCase{"y2045", 0}));

struct SetExpirationDateFromStringTestCase {
  std::string expiration_date;
  int expected_month;
  int expected_year;
};

class SetExpirationDateFromStringTest
    : public testing::TestWithParam<SetExpirationDateFromStringTestCase> {};

TEST_P(SetExpirationDateFromStringTest, SetExpirationDateFromString) {
  auto test_case = GetParam();
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "some origin");
  card.SetExpirationDateFromString(ASCIIToUTF16(test_case.expiration_date));

  EXPECT_EQ(test_case.expected_month, card.expiration_month());
  EXPECT_EQ(test_case.expected_year, card.expiration_year());
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    SetExpirationDateFromStringTest,
    testing::Values(
        SetExpirationDateFromStringTestCase{"10", 0, 0},       // Too small.
        SetExpirationDateFromStringTestCase{"1020451", 0, 0},  // Too long.

        // No separators.
        SetExpirationDateFromStringTestCase{"105", 0, 0},  // Too ambiguous.
        SetExpirationDateFromStringTestCase{"0545", 5, 2045},
        SetExpirationDateFromStringTestCase{"52045", 0, 0},  // Too ambiguous.
        SetExpirationDateFromStringTestCase{"052045", 5, 2045},

        // "/" separator.
        SetExpirationDateFromStringTestCase{"05/45", 5, 2045},
        SetExpirationDateFromStringTestCase{"5/2045", 5, 2045},
        SetExpirationDateFromStringTestCase{"05/2045", 5, 2045},
        SetExpirationDateFromStringTestCase{"05 / 45", 5, 2045},
        SetExpirationDateFromStringTestCase{"05 / 2045", 5, 2045},

        // "-" separator.
        SetExpirationDateFromStringTestCase{"05-45", 5, 2045},
        SetExpirationDateFromStringTestCase{"5-2045", 5, 2045},
        SetExpirationDateFromStringTestCase{"05-2045", 5, 2045},

        // "|" separator.
        SetExpirationDateFromStringTestCase{"05|45", 5, 2045},
        SetExpirationDateFromStringTestCase{"5|2045", 5, 2045},
        SetExpirationDateFromStringTestCase{"05|2045", 5, 2045},

        // Invalid values.
        SetExpirationDateFromStringTestCase{"13/2016", 0, 2016},
        SetExpirationDateFromStringTestCase{"16/13", 0, 2013},
        SetExpirationDateFromStringTestCase{"May-2015", 0, 0},
        SetExpirationDateFromStringTestCase{"05-/2045", 0, 0},
        SetExpirationDateFromStringTestCase{"05_2045", 0, 0}));

TEST(CreditCardTest, Copy) {
  CreditCard a(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               test::kEmptyOrigin);
  test::SetCreditCardInfo(&a, "John Dillinger", "123456789012", "01", "2010",
                          base::Uuid::GenerateRandomV4().AsLowercaseString());

  // Clone should be logically equal to the original.
  CreditCard b(a);
  EXPECT_TRUE(a == b);
}

struct IsLocalOrServerDuplicateOfTestCase {
  CreditCard::RecordType first_card_record_type;
  const char* first_card_name;
  const char* first_card_number;
  const char* first_card_exp_mo;
  const char* first_card_exp_yr;
  const char* first_billing_address_id;

  CreditCard::RecordType second_card_record_type;
  const char* second_card_name;
  const char* second_card_number;
  const char* second_card_exp_mo;
  const char* second_card_exp_yr;
  const char* second_billing_address_id;
  const char* second_card_issuer_network;

  bool is_local_or_server_duplicate;
};

class IsLocalOrServerDuplicateOfTest
    : public testing::TestWithParam<IsLocalOrServerDuplicateOfTestCase> {};

TEST_P(IsLocalOrServerDuplicateOfTest, IsLocalOrServerDuplicateOf) {
  auto test_case = GetParam();
  CreditCard a(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               std::string());
  a.set_record_type(test_case.first_card_record_type);
  test::SetCreditCardInfo(
      &a, test_case.first_card_name, test_case.first_card_number,
      test_case.first_card_exp_mo, test_case.first_card_exp_yr,
      test_case.first_billing_address_id);

  CreditCard b(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               std::string());
  b.set_record_type(test_case.second_card_record_type);
  test::SetCreditCardInfo(
      &b, test_case.second_card_name, test_case.second_card_number,
      test_case.second_card_exp_mo, test_case.second_card_exp_yr,
      test_case.second_billing_address_id);

  if (test_case.second_card_record_type ==
      CreditCard::RecordType::kMaskedServerCard) {
    b.SetNetworkForMaskedCard(test_case.second_card_issuer_network);
  }

  EXPECT_EQ(test_case.is_local_or_server_duplicate,
            a.IsLocalOrServerDuplicateOf(b))
      << " when comparing cards " << a.Label() << " and " << b.Label();
  // Flipping the checks for the cards to verify the functionality of
  // IsLocalOrServerDuplicateOf.
  EXPECT_EQ(test_case.is_local_or_server_duplicate,
            b.IsLocalOrServerDuplicateOf(a))
      << " when comparing cards " << b.Label() << " and " << a.Label();
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    IsLocalOrServerDuplicateOfTest,
    testing::Values(
        IsLocalOrServerDuplicateOfTestCase{LOCAL_CARD, "", "", "", "", "",
                                           LOCAL_CARD, "", "", "", "", "",
                                           nullptr, false},
        IsLocalOrServerDuplicateOfTestCase{LOCAL_CARD, "", "", "", "", "",
                                           FULL_SERVER_CARD, "", "", "", "", "",
                                           nullptr, true},
        IsLocalOrServerDuplicateOfTestCase{FULL_SERVER_CARD, "", "", "", "", "",
                                           FULL_SERVER_CARD, "", "", "", "", "",
                                           nullptr, false},
        IsLocalOrServerDuplicateOfTestCase{
            LOCAL_CARD, "John Dillinger", "423456789012", "01", "2010", "1",
            FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010",
            "1", nullptr, true},
        IsLocalOrServerDuplicateOfTestCase{
            LOCAL_CARD, "J Dillinger", "423456789012", "01", "2010", "1",
            FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010",
            "1", nullptr, false},
        IsLocalOrServerDuplicateOfTestCase{
            LOCAL_CARD, "", "423456789012", "01", "2010", "1", FULL_SERVER_CARD,
            "John Dillinger", "423456789012", "01", "2010", "1", nullptr, true},
        IsLocalOrServerDuplicateOfTestCase{
            LOCAL_CARD, "", "423456789012", "", "", "1", FULL_SERVER_CARD,
            "John Dillinger", "423456789012", "01", "2010", "1", nullptr, true},
        IsLocalOrServerDuplicateOfTestCase{
            LOCAL_CARD, "", "423456789012", "", "", "1", MASKED_SERVER_CARD,
            "John Dillinger", "9012", "01", "2010", "1", kVisaCard, true},
        IsLocalOrServerDuplicateOfTestCase{
            LOCAL_CARD, "John Dillinger", "4234-5678-9012", "01", "2010", "1",
            FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010",
            "1", nullptr, true},
        IsLocalOrServerDuplicateOfTestCase{
            LOCAL_CARD, "John Dillinger", "4234-5678-9012", "01", "2010", "1",
            FULL_SERVER_CARD, "John Dillinger", "423456789012", "01", "2010",
            "2", nullptr, false}));

TEST(CreditCardTest, MatchingCardDetails) {
  CreditCard a(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               std::string());
  CreditCard b(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               std::string());

  // Empty cards have the same empty number.
  EXPECT_TRUE(a.MatchingCardDetails(b));
  EXPECT_TRUE(b.MatchingCardDetails(a));

  // Cards with the same number are the same.
  a.set_record_type(CreditCard::RecordType::kLocalCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.set_record_type(CreditCard::RecordType::kLocalCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  EXPECT_TRUE(a.MatchingCardDetails(b));
  EXPECT_TRUE(b.MatchingCardDetails(a));

  // Local cards with different overall numbers shouldn't match even if the last
  // four digits are the same.
  a.set_record_type(CreditCard::RecordType::kLocalCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.set_record_type(CreditCard::RecordType::kLocalCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4111222222221111");
  EXPECT_FALSE(a.MatchingCardDetails(b));
  EXPECT_FALSE(b.MatchingCardDetails(a));

  // When one card is a full server card, the other is a local card, and the
  // cards have different overall numbers but the same last four digits, they
  // should not match.
  a.set_record_type(CreditCard::RecordType::kFullServerCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.set_record_type(CreditCard::RecordType::kLocalCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4111222222221111");
  EXPECT_FALSE(a.MatchingCardDetails(b));
  EXPECT_FALSE(b.MatchingCardDetails(a));

  // When one card is a masked server card, the other is a local card, and the
  // cards have the same last four digits, they should match.
  a.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.set_record_type(CreditCard::RecordType::kLocalCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4331111111111111");
  EXPECT_TRUE(a.MatchingCardDetails(b));
  EXPECT_TRUE(b.MatchingCardDetails(a));

  // When one card is a masked server card, the other is a full server card, and
  // the cards have the same last four digits, they should match.
  a.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.set_record_type(CreditCard::RecordType::kFullServerCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4331111111111111");
  EXPECT_TRUE(a.MatchingCardDetails(b));
  EXPECT_TRUE(b.MatchingCardDetails(a));

  // If one card is masked, then partial or missing expiration date information
  // should not prevent the function from returning true.
  a.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  a.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  a.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2025");
  b.set_record_type(CreditCard::RecordType::kLocalCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"");
  EXPECT_TRUE(a.MatchingCardDetails(b));
  EXPECT_TRUE(b.MatchingCardDetails(a));

  // If one card is masked, then non-matching expiration months should cause the
  // function to return false.
  a.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  a.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  a.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"");
  b.set_record_type(CreditCard::RecordType::kLocalCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"03");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"");
  EXPECT_FALSE(a.MatchingCardDetails(b));
  EXPECT_FALSE(b.MatchingCardDetails(a));

  // If one card is masked, then non-matching expiration years should cause the
  // function to return false.
  a.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  a.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  a.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"");
  a.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2025");
  b.set_record_type(CreditCard::RecordType::kLocalCard);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2026");
  EXPECT_FALSE(a.MatchingCardDetails(b));
  EXPECT_FALSE(b.MatchingCardDetails(a));
}

TEST(CreditCardTest, IsVerified) {
  CreditCard card;
  EXPECT_FALSE(card.IsVerified());

  card.set_origin("http://www.example.com");
  EXPECT_FALSE(card.IsVerified());

  card.set_origin("https://www.example.com");
  EXPECT_FALSE(card.IsVerified());

  card.set_origin("file:///tmp/example.txt");
  EXPECT_FALSE(card.IsVerified());

  card.set_origin("data:text/plain;charset=utf-8;base64,ZXhhbXBsZQ==");
  EXPECT_FALSE(card.IsVerified());

  card.set_origin("chrome://settings/autofill");
  EXPECT_FALSE(card.IsVerified());

  card.set_origin(kSettingsOrigin);
  EXPECT_TRUE(card.IsVerified());

  card.set_origin("Some gibberish string");
  EXPECT_TRUE(card.IsVerified());

  card.set_origin(std::string());
  EXPECT_FALSE(card.IsVerified());
}

TEST(CreditCardTest, Compare) {
  CreditCard a(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               std::string());
  CreditCard b(base::Uuid::GenerateRandomV4().AsLowercaseString(),
               std::string());

  // Empty cards are the same.
  EXPECT_EQ(0, a.Compare(b));

  // GUIDs don't count.
  a.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  EXPECT_EQ(0, a.Compare(b));

  // Origins don't count.
  a.set_origin("apple");
  b.set_origin("banana");
  EXPECT_EQ(0, a.Compare(b));

  // Different types of server cards don't count.
  a.set_record_type(MASKED_SERVER_CARD);
  b.set_record_type(FULL_SERVER_CARD);
  EXPECT_EQ(0, a.Compare(b));

  // Difference in nickname counts.
  a.SetNickname(u"My Visa Card");
  b.SetNickname(u"Grocery Cashback Card");
  EXPECT_LT(0, a.Compare(b));
  // Reset the nickname to empty, empty nickname cards are the same.
  a.SetNickname(u"");
  b.SetNickname(u"");
  EXPECT_EQ(0, a.Compare(b));

  // Local is different from server.
  a.set_record_type(LOCAL_CARD);
  b.set_record_type(FULL_SERVER_CARD);
  EXPECT_GT(0, a.Compare(b));
  a.set_record_type(MASKED_SERVER_CARD);
  b.set_record_type(LOCAL_CARD);
  EXPECT_LT(0, a.Compare(b));
  a.set_record_type(MASKED_SERVER_CARD);
  b.set_record_type(MASKED_SERVER_CARD);

  // Card with UNKNOWN_ISSUER is different from GOOGLE issued card.
  a.set_card_issuer(CreditCard::Issuer::kIssuerUnknown);
  b.set_card_issuer(CreditCard::Issuer::kGoogle);
  EXPECT_GT(0, a.Compare(b));
  // Card with UNKNOWN_ISSUER is different from EXTERNAL_ISSUER issued card.
  a.set_card_issuer(CreditCard::Issuer::kIssuerUnknown);
  b.set_card_issuer(CreditCard::Issuer::kExternalIssuer);
  EXPECT_GT(0, a.Compare(b));
  a.set_card_issuer(CreditCard::Issuer::kExternalIssuer);
  b.set_card_issuer(CreditCard::Issuer::kExternalIssuer);

  // Difference in issuer id.
  a.set_issuer_id("amex");
  b.set_issuer_id("capitalone");
  EXPECT_NE(0, a.Compare(b));
  // Reset the issuer ids to empty, and empty ids are considered the same.
  a.set_issuer_id("");
  b.set_issuer_id("");
  EXPECT_EQ(0, a.Compare(b));

  // Difference in cvc.
  a.set_cvc(u"1234");
  b.set_cvc(u"987");
  EXPECT_NE(0, a.Compare(b));
  // Card with cvc is different from card with empty cvc.
  a.set_cvc(u"1234");
  b.set_cvc(u"");
  EXPECT_NE(0, a.Compare(b));
  // Reset the cvc to empty, and empty cvc are considered the same.
  a.set_cvc(u"");
  b.set_cvc(u"");
  EXPECT_EQ(0, a.Compare(b));
  // Two same non-empty cvc are considered the same.
  a.set_cvc(u"123");
  b.set_cvc(u"123");
  EXPECT_EQ(0, a.Compare(b));

  // Different values produce non-zero results.
  test::SetCreditCardInfo(&a, "Jimmy", nullptr, nullptr, nullptr, "");
  test::SetCreditCardInfo(&b, "Ringo", nullptr, nullptr, nullptr, "");
  EXPECT_GT(0, a.Compare(b));
  EXPECT_LT(0, b.Compare(a));
}

// Test we get the correct icon for each card type.
TEST(CreditCardTest, IconResourceId) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVerveCardSupport};
  bool new_network_images = base::FeatureList::IsEnabled(
      features::kAutofillEnableNewCardArtAndNetworkImages);
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_AMEX : IDR_AUTOFILL_CC_AMEX,
      CreditCard::IconResourceId(Suggestion::Icon::kCardAmericanExpress));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_DINERS
                               : IDR_AUTOFILL_CC_DINERS,
            CreditCard::IconResourceId(Suggestion::Icon::kCardDiners));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_DISCOVER
                               : IDR_AUTOFILL_CC_DISCOVER,
            CreditCard::IconResourceId(Suggestion::Icon::kCardDiscover));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_ELO : IDR_AUTOFILL_CC_ELO,
      CreditCard::IconResourceId(Suggestion::Icon::kCardElo));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_JCB : IDR_AUTOFILL_CC_JCB,
      CreditCard::IconResourceId(Suggestion::Icon::kCardJCB));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_MASTERCARD
                               : IDR_AUTOFILL_CC_MASTERCARD,
            CreditCard::IconResourceId(Suggestion::Icon::kCardMasterCard));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_MIR : IDR_AUTOFILL_CC_MIR,
      CreditCard::IconResourceId(Suggestion::Icon::kCardMir));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_TROY : IDR_AUTOFILL_CC_TROY,
      CreditCard::IconResourceId(Suggestion::Icon::kCardTroy));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_UNIONPAY
                               : IDR_AUTOFILL_CC_UNIONPAY,
            CreditCard::IconResourceId(Suggestion::Icon::kCardUnionPay));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_VERVE
                               : IDR_AUTOFILL_CC_VERVE,
            CreditCard::IconResourceId(Suggestion::Icon::kCardVerve));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_VISA : IDR_AUTOFILL_CC_VISA,
      CreditCard::IconResourceId(Suggestion::Icon::kCardVisa));
}

// Test we get the correct icon for each card type.
TEST(CreditCardTest, IconResourceIdFromString) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVerveCardSupport};
  bool new_network_images = base::FeatureList::IsEnabled(
      features::kAutofillEnableNewCardArtAndNetworkImages);
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_AMEX : IDR_AUTOFILL_CC_AMEX,
      CreditCard::IconResourceId(kAmericanExpressCard));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_DINERS
                               : IDR_AUTOFILL_CC_DINERS,
            CreditCard::IconResourceId(kDinersCard));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_DISCOVER
                               : IDR_AUTOFILL_CC_DISCOVER,
            CreditCard::IconResourceId(kDiscoverCard));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_ELO : IDR_AUTOFILL_CC_ELO,
      CreditCard::IconResourceId(kEloCard));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_JCB : IDR_AUTOFILL_CC_JCB,
      CreditCard::IconResourceId(kJCBCard));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_MASTERCARD
                               : IDR_AUTOFILL_CC_MASTERCARD,
            CreditCard::IconResourceId(kMasterCard));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_MIR : IDR_AUTOFILL_CC_MIR,
      CreditCard::IconResourceId(kMirCard));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_TROY : IDR_AUTOFILL_CC_TROY,
      CreditCard::IconResourceId(kTroyCard));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_UNIONPAY
                               : IDR_AUTOFILL_CC_UNIONPAY,
            CreditCard::IconResourceId(kUnionPay));
  EXPECT_EQ(new_network_images ? IDR_AUTOFILL_METADATA_CC_VERVE
                               : IDR_AUTOFILL_CC_VERVE,
            CreditCard::IconResourceId(kVerveCard));
  EXPECT_EQ(
      new_network_images ? IDR_AUTOFILL_METADATA_CC_VISA : IDR_AUTOFILL_CC_VISA,
      CreditCard::IconResourceId(kVisaCard));
}

TEST(CreditCardTest, UpdateFromImportedCard_UpdatedWithNameAndExpirationDate) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // The new card has a different name, expiration date.
  CreditCard b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, u"J. Dillinger");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  // |a| should be updated with the information from |b|.
  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(test::kEmptyOrigin, a.origin());
  EXPECT_EQ(u"J. Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"08", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(kYearInFuture, a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_UpdatedWithNameAndInvalidExpirationDateMonth) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // The new card has a different name and empty origin and invalid expiration
  // date month
  // |a| should be updated with |b|'s name and keep its original expiration
  // date.
  CreditCard b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, u"J. Dillinger");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"0");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(test::kEmptyOrigin, a.origin());
  EXPECT_EQ(u"J. Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2017", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_UpdatedWithNameAndInvalidExpirationDateYear) {
  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");

  CreditCard a = original_card;

  // The new card has a different name and empty origin and invalid expiration
  // date year
  // |a| should be updated with |b|'s name and keep its original expiration
  // date.
  CreditCard b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_origin(test::kEmptyOrigin);
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, u"J. Dillinger");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"09");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"");

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(test::kEmptyOrigin, a.origin());
  EXPECT_EQ(u"J. Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2017", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_UpdatedWithEmptyNameAndValidExpirationDate) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // A valid new expiration date and no name set for |b|.
  // |a| should be updated with |b|'s expiration date and keep its original
  // name.
  CreditCard b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, std::u16string());
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(test::kEmptyOrigin, a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"08", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(kYearInFuture, a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(
    CreditCardTest,
    UpdateFromImportedCard_VerifiedCardNotUpdatedWithEmptyExpirationDateMonth) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");

  CreditCard a = original_card;

  // Empty expiration date month set for |b| and original card verified.
  // |a| should be unchanged.
  CreditCard b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  a.set_origin("Chrome settings");
  b.set_origin(test::kEmptyOrigin);
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, u"J. Dillinger");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"0");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2017", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_VerifiedCardNotUpdatedWithEmptyExpirationDateYear) {
  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // Empty expiration date year set for |b| and original card verified.
  // |a| should be unchanged.
  CreditCard b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  a.set_origin("Chrome settings");
  b.set_origin(test::kEmptyOrigin);
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, u"J. Dillinger");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"09");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"0");

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2017", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_VerifiedCardNotUpdatedWithDifferentName) {
  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // New card is from empty origin and has an different name.
  // |a| should be unchanged.
  CreditCard b = a;
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  a.set_origin(kSettingsOrigin);
  b.set_origin(test::kEmptyOrigin);
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, u"J. Dillinger");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2017");

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ(kSettingsOrigin, a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2017", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_ExpiredVerifiedCardNotUpdatedWithDifferentName) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // New card is from empty origin and has a different name.
  // |a| is an expired verified original card and should not be updated because
  // the name on the cards are not identical with |b|.
  CreditCard b = a;
  a.set_origin("Chrome settings");
  a.SetExpirationYear(2010);
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_origin(test::kEmptyOrigin);
  b.SetRawInfo(CREDIT_CARD_NAME_FULL, u"J. Dillinger");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2010", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_ExpiredVerifiedCardUpdatedWithSameName) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // New card is from empty origin and has a same name.
  // |a| is an expired verified original card.
  // |a|'s expiration date should be updated.
  CreditCard b = a;
  a.set_origin("Chrome settings");
  a.SetExpirationYear(2010);
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_origin(test::kEmptyOrigin);
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"08", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(kYearInFuture, a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_ExpiredOriginalCardVerifiedUpdatedWithExpiredCard) {
  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // New card is expired and from empty origin.
  // |a| is an expired verified original card.
  // |a|'s expiration date should not be updated.
  CreditCard b = a;
  a.set_origin("Chrome settings");
  a.SetExpirationYear(2010);
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_origin(test::kEmptyOrigin);
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2009");

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2010", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_VerifiedCardUpdatedWithVerifiedCard) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // New card is verified.
  // |a| is an expired verified original card.
  // |a|'s expiration date should be updated.
  CreditCard b = a;
  a.set_origin("Chrome settings");
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_origin(kSettingsOrigin);
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  EXPECT_TRUE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"08", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(kYearInFuture, a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest,
     UpdateFromImportedCard_VerifiedCardNotUpdatedWithDifferentCard) {
  const std::u16string kYearInFuture = GetYearInTheFuture();

  CreditCard original_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                           test::kEmptyOrigin);
  test::SetCreditCardInfo(&original_card, "John Dillinger", "123456789012",
                          "09", "2017", "1");
  CreditCard a = original_card;

  // New card has diffenrent card number.
  // |a| is an expired verified original card.
  // |a|'s expiration date should be updated.
  CreditCard b = a;
  a.set_origin("Chrome settings");
  b.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  b.set_origin(kSettingsOrigin);
  b.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  b.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"08");
  b.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, kYearInFuture);

  EXPECT_FALSE(a.UpdateFromImportedCard(b, "en-US"));
  EXPECT_EQ("Chrome settings", a.origin());
  EXPECT_EQ(u"John Dillinger", a.GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"09", a.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(u"2017", a.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST(CreditCardTest, IsValidCardNumberAndExpiryDate) {
  CreditCard card;
  // Invalid because expired
  const base::Time now(AutofillClock::Now());
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH,
                  base::NumberToString16(now_exploded.month));
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                  base::NumberToString16(now_exploded.year - 1));
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  EXPECT_FALSE(card.IsValid());
  EXPECT_FALSE(card.HasValidExpirationDate());
  EXPECT_TRUE(card.HasValidCardNumber());

  // Invalid because card number is not complete
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2999");
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"41111");
  EXPECT_FALSE(card.IsValid());

  for (const char* valid_number : kValidNumbers) {
    SCOPED_TRACE(valid_number);
    card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16(valid_number));
    EXPECT_TRUE(card.IsValid());
    EXPECT_TRUE(card.HasValidCardNumber());
    EXPECT_TRUE(card.HasValidExpirationDate());
  }
  for (const char* invalid_number : kInvalidNumbers) {
    SCOPED_TRACE(invalid_number);
    card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16(invalid_number));
    EXPECT_FALSE(card.IsValid());
    EXPECT_TRUE(card.HasValidExpirationDate());
    EXPECT_FALSE(card.HasValidCardNumber());
  }
}

TEST(CreditCardTest, HasNonEmptyValidNickname) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");
  test::SetCreditCardInfo(&card, "John Dillinger", "5105 1051 0510 5100", "01",
                          "2020", "1");

  for (const char* valid_nickname : kValidNicknames) {
    SCOPED_TRACE(valid_nickname);
    card.SetNickname(UTF8ToUTF16(valid_nickname));
    EXPECT_TRUE(card.HasNonEmptyValidNickname());
  }
  for (const char* invalid_nickname : kInvalidNicknames) {
    SCOPED_TRACE(invalid_nickname);
    card.SetNickname(UTF8ToUTF16(invalid_nickname));
    EXPECT_FALSE(card.HasNonEmptyValidNickname());
  }

  // HasNonEmptyValidNickname should return false if nickname is empty.
  {
    SCOPED_TRACE(kEmptyNickname);
    card.SetNickname(UTF8ToUTF16(kEmptyNickname));
    EXPECT_FALSE(card.HasNonEmptyValidNickname());
  }
}

TEST(CreditCardTest, IsNicknameValid) {
  for (const char* valid_nickname : kValidNicknames) {
    SCOPED_TRACE(valid_nickname);
    EXPECT_TRUE(CreditCard::IsNicknameValid(UTF8ToUTF16(valid_nickname)));
  }

  // IsNicknameValid should return true if nickname is empty.
  {
    SCOPED_TRACE(kEmptyNickname);
    EXPECT_TRUE(CreditCard::IsNicknameValid(UTF8ToUTF16(kEmptyNickname)));
  }

  for (const char* invalid_nickname : kInvalidNicknames) {
    SCOPED_TRACE(invalid_nickname);
    EXPECT_FALSE(CreditCard::IsNicknameValid(UTF8ToUTF16(invalid_nickname)));
  }
}

// Verify that we preserve exactly what the user typed for credit card numbers.
TEST(CreditCardTest, SetRawInfoCreditCardNumber) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");

  test::SetCreditCardInfo(&card, "Bob Dylan", "4321-5432-6543-xxxx", "07",
                          "2013", "1");
  EXPECT_EQ(u"4321-5432-6543-xxxx", card.GetRawInfo(CREDIT_CARD_NUMBER));
}

// Verify that we can handle both numeric and named months.
TEST(CreditCardTest, SetExpirationMonth) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");

  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"05");
  EXPECT_EQ(u"05", card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(5, card.expiration_month());

  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"7");
  EXPECT_EQ(u"07", card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(7, card.expiration_month());

  // This should fail, and preserve the previous value.
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"January");
  EXPECT_EQ(u"07", card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(7, card.expiration_month());

  card.SetInfo(CREDIT_CARD_EXP_MONTH, u"January", "en-US");
  EXPECT_EQ(u"01", card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(1, card.expiration_month());

  card.SetInfo(CREDIT_CARD_EXP_MONTH, u"Apr", "en-US");
  EXPECT_EQ(u"04", card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(4, card.expiration_month());

  card.SetInfo(CREDIT_CARD_EXP_MONTH, u"FÉVRIER", "fr-FR");
  EXPECT_EQ(u"02", card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(2, card.expiration_month());
}

// Verify that we set nickname with the processed string. We replace all tabs
// and newlines with whitespace, and trim leading/trailing whitespace.
TEST(CreditCardTest, SetNickname) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");

  // Normal input nickname.
  card.SetNickname(u"Grocery card");
  EXPECT_EQ(u"Grocery card", card.nickname());

  // Input nickname has leading and trailing whitespaces.
  card.SetNickname(u"  Grocery card  ");
  EXPECT_EQ(u"Grocery card", card.nickname());

  // Input nickname has newlines.
  card.SetNickname(u"\r\n Grocery\ncard \r\n");
  EXPECT_EQ(u"Grocery card", card.nickname());

  // Input nickname has tabs.
  card.SetNickname(u" \tGrocery\t card\t ");
  EXPECT_EQ(u"Grocery  card", card.nickname());

  // Input nickname has newlines & whitespaces & tabs.
  card.SetNickname(u"\n\t Grocery \tcard \n \r\n");
  EXPECT_EQ(u"Grocery  card", card.nickname());
}

TEST(CreditCardTest, CreditCardType) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");

  // The card type cannot be set directly.
  card.SetRawInfo(CREDIT_CARD_TYPE, u"Visa");
  EXPECT_EQ(std::u16string(), card.GetRawInfo(CREDIT_CARD_TYPE));

  // Setting the number should implicitly set the type.
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"4111 1111 1111 1111");
  EXPECT_EQ(u"Visa", card.GetRawInfo(CREDIT_CARD_TYPE));
}

// Verify that we preserve exactly what the user typed for CVC.
TEST(CreditCardTest, CreditCardVerificationCode) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");
  card.SetRawInfo(CREDIT_CARD_VERIFICATION_CODE, u"999");
  EXPECT_EQ(u"999", card.GetRawInfo(CREDIT_CARD_VERIFICATION_CODE));
  EXPECT_EQ(u"999", card.cvc());
  card.SetRawInfo(CREDIT_CARD_STANDALONE_VERIFICATION_CODE, u"999");
  EXPECT_EQ(u"999", card.GetRawInfo(CREDIT_CARD_STANDALONE_VERIFICATION_CODE));
  EXPECT_EQ(u"999", card.cvc());
}

// Tests that the card in only deletable if it is expired before the threshold.
TEST(CreditCardTest, IsDeletable) {
  // Set up an arbitrary time, as setup the current time to just above the
  // threshold later than that time. This sets the year to 2007. The code
  // expects valid expiration years to be between 2000 and 2999. However,
  // because of the year 2018 problem, we need to pick an earlier year.
  const base::Time kArbitraryTime =
      base::Time::FromSecondsSinceUnixEpoch(1000000000);
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime + kDisusedDataModelDeletionTimeDelta +
                    base::Days(1));

  // Created a card that has not been used since over the deletion threshold.
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");
  card.set_use_date(kArbitraryTime);

  // Set the card to be expired before the threshold.
  base::Time::Exploded now_exploded;
  AutofillClock::Now().LocalExplode(&now_exploded);
  card.SetExpirationYear(now_exploded.year - 5);
  card.SetExpirationMonth(1);
  ASSERT_TRUE(card.IsExpired(AutofillClock::Now() -
                             kDisusedDataModelDeletionTimeDelta));

  // Make sure the card is deletable.
  EXPECT_TRUE(card.IsDeletable());

  // Set the card to not be expired.
  card.SetExpirationYear(now_exploded.year + 5);
  ASSERT_FALSE(card.IsExpired(AutofillClock::Now() -
                              kDisusedDataModelDeletionTimeDelta));

  // Make sure the card is not deletable.
  EXPECT_FALSE(card.IsDeletable());
}

struct CreditCardMatchingTypesCase {
  CreditCardMatchingTypesCase(const char* value,
                              const char* card_exp_month,
                              const char* card_exp_year,
                              CreditCard::RecordType record_type,
                              FieldTypeSet expected_matched_types,
                              const char* locale = "US")
      : value(value),
        card_exp_month(card_exp_month),
        card_exp_year(card_exp_year),
        record_type(record_type),
        expected_matched_types(expected_matched_types),
        locale(locale) {}

  // The value entered by the user.
  const char* value;
  // Some values for an already saved card. Card number will be fixed to
  // 4012888888881881.
  const char* card_exp_month;
  const char* card_exp_year;
  const CreditCard::RecordType record_type;
  // The types that are expected to match.
  const FieldTypeSet expected_matched_types;

  const char* locale = "US";
};

class CreditCardMatchingTypesTest
    : public testing::TestWithParam<CreditCardMatchingTypesCase> {};

TEST_P(CreditCardMatchingTypesTest, Cases) {
  auto test_case = GetParam();
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");
  card.set_record_type(test_case.record_type);
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"4012888888881881");
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH,
                  ASCIIToUTF16(test_case.card_exp_month));
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                  ASCIIToUTF16(test_case.card_exp_year));

  FieldTypeSet matching_types;
  card.GetMatchingTypesWithProfileSources(
      UTF8ToUTF16(test_case.value), test_case.locale, &matching_types, nullptr);
  EXPECT_EQ(test_case.expected_matched_types, matching_types);
}

const CreditCardMatchingTypesCase kCreditCardMatchingTypesTestCases[] = {
    // If comparing against a masked card, last four digits are checked.
    {"1881", "01", "2020", MASKED_SERVER_CARD, {CREDIT_CARD_NUMBER}},
    {"4012888888881881",
     "01",
     "2020",
     MASKED_SERVER_CARD,
     {CREDIT_CARD_NUMBER}},
    {"4111111111111111", "01", "2020",
     CreditCard::RecordType::kMaskedServerCard, FieldTypeSet()},
    // Same value will not match a local card or full server card since we
    // have the full number for those. However the full number will.
    {"1881", "01", "2020", LOCAL_CARD, FieldTypeSet()},
    {"1881", "01", "2020", FULL_SERVER_CARD, FieldTypeSet()},
    {"4012888888881881", "01", "2020", LOCAL_CARD, {CREDIT_CARD_NUMBER}},
    {"4012888888881881", "01", "2020", FULL_SERVER_CARD, {CREDIT_CARD_NUMBER}},

    // Wrong last four digits.
    {"1111", "01", "2020", MASKED_SERVER_CARD, FieldTypeSet()},
    {"1111", "01", "2020", LOCAL_CARD, FieldTypeSet()},
    {"1111", "01", "2020", FULL_SERVER_CARD, FieldTypeSet()},
    {"4111111111111111", "01", "2020", MASKED_SERVER_CARD, FieldTypeSet()},
    {"4111111111111111", "01", "2020", LOCAL_CARD, FieldTypeSet()},
    {"4111111111111111", "01", "2020", FULL_SERVER_CARD, FieldTypeSet()},

    // Matching the expiration month.
    {"01", "01", "2020", LOCAL_CARD, {CREDIT_CARD_EXP_MONTH}},
    {"1", "01", "2020", LOCAL_CARD, {CREDIT_CARD_EXP_MONTH}},
    {"jan", "01", "2020", LOCAL_CARD, {CREDIT_CARD_EXP_MONTH}, "US"},
    // Locale-specific interpretations.
    {"janv", "01", "2020", LOCAL_CARD, {CREDIT_CARD_EXP_MONTH}, "FR"},
    {"janv.", "01", "2020", LOCAL_CARD, {CREDIT_CARD_EXP_MONTH}, "FR"},
    {"janvier", "01", "2020", LOCAL_CARD, {CREDIT_CARD_EXP_MONTH}, "FR"},
    {"février", "02", "2020", LOCAL_CARD, {CREDIT_CARD_EXP_MONTH}, "FR"},
    {"mars", "01", "2020", LOCAL_CARD, FieldTypeSet(), "FR"},

    // Matching the expiration year.
    {"2019", "01", "2019", LOCAL_CARD, {CREDIT_CARD_EXP_4_DIGIT_YEAR}},
    {"19", "01", "2019", LOCAL_CARD, {CREDIT_CARD_EXP_2_DIGIT_YEAR}},
    {"01/2019", "01", "2019", LOCAL_CARD, {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}},
    {"01-2019", "01", "2019", LOCAL_CARD, {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}},
    {"01/19", "01", "2019", LOCAL_CARD, {CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}},
    {"01-19", "01", "2019", LOCAL_CARD, {CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}},
    {"01 / 19", "01", "2019", LOCAL_CARD, {CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR}},
    {"01/2020", "01", "2019", LOCAL_CARD, FieldTypeSet()},
    {"20", "01", "2019", LOCAL_CARD, FieldTypeSet()},
    {"2021", "01", "2019", LOCAL_CARD, FieldTypeSet()},
};

INSTANTIATE_TEST_SUITE_P(CreditCardTest,
                         CreditCardMatchingTypesTest,
                         testing::ValuesIn(kCreditCardMatchingTypesTestCases));

TEST(CreditCardTest, LastFourDigits) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");
  ASSERT_EQ(std::u16string(), card.LastFourDigits());
  ASSERT_EQ(CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/4, std::u16string()),
            card.ObfuscatedNumberWithVisibleLastFourDigits());

  test::SetCreditCardInfo(&card, "Baby Face Nelson", "5212341234123489", "01",
                          "2010", "1");
  ASSERT_EQ(u"3489", card.LastFourDigits());
  ASSERT_EQ(CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/4, u"3489"),
            card.ObfuscatedNumberWithVisibleLastFourDigits());

  card.SetRawInfo(CREDIT_CARD_NUMBER, u"3489");
  ASSERT_EQ(u"3489", card.LastFourDigits());
  ASSERT_EQ(CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/4, u"3489"),
            card.ObfuscatedNumberWithVisibleLastFourDigits());

  card.SetRawInfo(CREDIT_CARD_NUMBER, u"489");
  ASSERT_EQ(u"489", card.LastFourDigits());
  ASSERT_EQ(CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/4, u"489"),
            card.ObfuscatedNumberWithVisibleLastFourDigits());
}

TEST(CreditCardTest, FullDigitsForDisplay) {
  CreditCard card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  "https://www.example.com/");
  ASSERT_EQ(std::u16string(), card.FullDigitsForDisplay());

  test::SetCreditCardInfo(&card, "Baby Face Nelson", "5212341234123489", "01",
                          "2010", "1");
  ASSERT_EQ(u"5212 3412 3412 3489", card.FullDigitsForDisplay());

  // Unstripped card number.
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"5212-3412-3412-3489");
  ASSERT_EQ(u"5212 3412 3412 3489", card.FullDigitsForDisplay());

  // 15-digit Amex card number adds spacing.
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"378282246310005");
  ASSERT_EQ(u"3782 822463 10005", card.FullDigitsForDisplay());

  // 16-digit card number that begins with Amex digits should have normal
  // spacing.
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"3782822463100052");
  ASSERT_EQ(u"3782 8224 6310 0052", card.FullDigitsForDisplay());

  // 15-digit non-Amex card number stays the same.
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"998282246310005");
  ASSERT_EQ(u"998282246310005", card.FullDigitsForDisplay());

  // 19-digit card number stays the same.
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"4532261615476013542");
  ASSERT_EQ(u"4532261615476013542", card.FullDigitsForDisplay());

  // Ideally FullDigitsForDisplay shouldn't be invoked for masked cards.Test
  // here just in case. Masked card stays the same.
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"3489");
  ASSERT_EQ(u"3489", card.FullDigitsForDisplay());
}

// Verifies that a credit card should be updated.
struct ShouldUpdateExpirationTestCase {
  bool should_update_expiration;
  base::TimeDelta time_delta;
  CreditCard::RecordType record_type;
};

constexpr base::TimeDelta kCurrent = base::Days(0);
constexpr base::TimeDelta kOneYear = base::Days(365);
constexpr base::TimeDelta kOneMonth = base::Days(31);

void MonthAndYearFromDelta(base::TimeDelta time_delta, int& month, int& year) {
  base::Time now = AutofillClock::Now();
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(now);
  base::Time::Exploded exploded;
  (now + time_delta).LocalExplode(&exploded);
  month = exploded.month;
  year = exploded.year;
}

class ShouldUpdateExpirationTest
    : public testing::TestWithParam<ShouldUpdateExpirationTestCase> {};

TEST_P(ShouldUpdateExpirationTest, ShouldUpdateExpiration) {
  auto test_case = GetParam();
  CreditCard card;
  int month, year;
  MonthAndYearFromDelta(test_case.time_delta, month, year);
  card.SetExpirationMonth(month);
  card.SetExpirationYear(year);
  card.set_record_type(test_case.record_type);

  EXPECT_EQ(test_case.should_update_expiration, card.ShouldUpdateExpiration());
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    ShouldUpdateExpirationTest,
    testing::Values(
        // Cards that expired last year should always be updated.
        ShouldUpdateExpirationTestCase{true, -kOneYear,
                                       CreditCard::RecordType::kLocalCard},
        ShouldUpdateExpirationTestCase{true, -kOneYear,
                                       CreditCard::RecordType::kFullServerCard},
        ShouldUpdateExpirationTestCase{
            true, -kOneYear, CreditCard::RecordType::kMaskedServerCard},

        // Cards that expired last month should always be updated.
        ShouldUpdateExpirationTestCase{true, -kOneMonth,
                                       CreditCard::RecordType::kLocalCard},
        ShouldUpdateExpirationTestCase{true, -kOneMonth,
                                       CreditCard::RecordType::kFullServerCard},
        ShouldUpdateExpirationTestCase{
            true, -kOneMonth, CreditCard::RecordType::kMaskedServerCard},

        // Cards that expire this month should not be updated.
        ShouldUpdateExpirationTestCase{false, kCurrent,
                                       CreditCard::RecordType::kLocalCard},
        ShouldUpdateExpirationTestCase{false, kCurrent,
                                       CreditCard::RecordType::kFullServerCard},
        ShouldUpdateExpirationTestCase{
            false, kCurrent, CreditCard::RecordType::kMaskedServerCard},

        // Cards that expire next month should not be updated.
        ShouldUpdateExpirationTestCase{false, kOneMonth,
                                       CreditCard::RecordType::kLocalCard},
        ShouldUpdateExpirationTestCase{
            false, kOneMonth, CreditCard::RecordType::kMaskedServerCard},
        ShouldUpdateExpirationTestCase{false, kOneMonth,
                                       CreditCard::RecordType::kFullServerCard},

        // Cards that expire next year should not be updated.
        ShouldUpdateExpirationTestCase{false, kOneYear,
                                       CreditCard::RecordType::kLocalCard},
        ShouldUpdateExpirationTestCase{
            false, kOneYear, CreditCard::RecordType::kMaskedServerCard},
        ShouldUpdateExpirationTestCase{
            false, kOneYear, CreditCard::RecordType::kFullServerCard}));

#if BUILDFLAG(IS_ANDROID)
TEST(CreditCardTestForKeyboardAccessory, GetObfuscatedStringForCardDigits) {
  const std::u16string digits = u"1235";
  const std::u16string expected =
      std::u16string() + base::i18n::kLeftToRightEmbeddingMark +
      CreditCard::GetMidlineEllipsisDots(2) + digits +
      base::i18n::kPopDirectionalFormatting;

  EXPECT_EQ(expected, CreditCard::GetObfuscatedStringForCardDigits(
                          /*obfuscation_length=*/2, digits));
}
#endif  // BUILDFLAG(IS_ANDROID)

enum Expectation { GREATER, LESS };

struct VirtualCardRankingTestCase {
  const std::string guid_a;
  const int use_count_a;
  const base::Time use_date_a;
  const std::string guid_b;
  const int use_count_b;
  const base::Time use_date_b;
  Expectation expectation;
};

base::Time current_time = AutofillClock::Now();

class VirtualCardRankingTest
    : public testing::TestWithParam<VirtualCardRankingTestCase> {};

TEST_P(VirtualCardRankingTest, HasGreaterRankingThan) {
  // Enable kAutofillEnableRankingFormulaCreditCards so that it uses new formula
  // instead of frecency.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableRankingFormulaCreditCards);

  auto test_case = GetParam();

  CreditCard model_a = test::GetVirtualCard();
  model_a.set_guid(test_case.guid_a);
  model_a.set_use_count(test_case.use_count_a);
  model_a.set_use_date(test_case.use_date_a);

  CreditCard model_b = test::GetCreditCard();
  model_b.set_guid(test_case.guid_b);
  model_b.set_use_count(test_case.use_count_b);
  model_b.set_use_date(test_case.use_date_b);

  EXPECT_EQ(test_case.expectation == GREATER,
            model_a.HasGreaterRankingThan(model_b, current_time));
  EXPECT_NE(test_case.expectation == GREATER,
            model_b.HasGreaterRankingThan(model_a, current_time));
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    VirtualCardRankingTest,
    testing::Values(
        // Same days since last use and use count, but model A is a virtual card
        // and ranked higher.
        VirtualCardRankingTestCase{"guid_a", 10, current_time, "guid_b", 10,
                                   current_time, GREATER},
        // Same days since last use and although model A has a smaller use
        // count, it is a virtual card and ranked higher.
        VirtualCardRankingTestCase{"guid_a", 1, current_time, "guid_b", 10,
                                   current_time, GREATER},
        // Model A has a larger use count but smaller days since last use. model
        // A is ranked higher due to virtual card boost.
        VirtualCardRankingTestCase{"guid_a", 10, current_time - base::Days(10),
                                   "guid_b", 5, current_time, GREATER},
        // Model A has a larger use count but also a much larger days since last
        // use. Due to this, model B is ranked higher despite the virtual card
        // boost and greater use count of model A.
        VirtualCardRankingTestCase{"guid_a", 10, current_time - base::Days(40),
                                   "guid_b", 3, current_time, LESS},
        // Model A only has a use count of 1 but due to its virtual card bost
        // and much smaller days since last use it is ranked higher than model
        // B.
        VirtualCardRankingTestCase{"guid_a", 1, current_time - base::Days(30),
                                   "guid_b", 300, current_time - base::Days(90),
                                   GREATER},
        // Model B only has a use count of 1 but due to its much smaller day
        // since last use it is ranked higher than model A despite its virtual
        // card boost and much higher use count.
        VirtualCardRankingTestCase{"guid_a", 300, current_time - base::Days(90),
                                   "guid_b", 1, current_time - base::Days(30),
                                   LESS}));

}  // namespace
}  // namespace autofill
