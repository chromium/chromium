// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(BankAccountTest, VerifyAllFields) {
  BankAccount bank_account(100, u"nickname", GURL("http://www.example.com"),
                           u"bank_name", u"account_number_suffix",
                           BankAccount::AccountType::kChecking);

  EXPECT_EQ(100, bank_account.payment_instrument().instrument_id());
  EXPECT_EQ(u"bank_name", bank_account.bank_name());
  EXPECT_EQ(u"account_number_suffix", bank_account.account_number_suffix());
  EXPECT_EQ(u"nickname", bank_account.payment_instrument().nickname());
  EXPECT_EQ(GURL("http://www.example.com"),
            bank_account.payment_instrument().display_icon_url());
  EXPECT_EQ(BankAccount::AccountType::kChecking, bank_account.account_type());
  EXPECT_TRUE(bank_account.payment_instrument().IsSupported(
      PaymentInstrument::PaymentRail::kPix));
}

TEST(BankAccountTest, Compare_DifferentPaymentInstrument) {
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kChecking);

  BankAccount bank_account_2(200, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kChecking);

  // bank_account_1 is smaller than bank_account_2 because 100 < 200, thus
  // expect the output to be -1.
  EXPECT_EQ(-1, bank_account_1.Compare(bank_account_2));
}

TEST(BankAccountTest, Compare_BankNameSmaller) {
  std::u16string bank_name_1 = u"bank_name_1";
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             bank_name_1, u"account_number_suffix",
                             BankAccount::AccountType::kChecking);
  std::u16string bank_name_2 = u"bank_name_2";
  BankAccount bank_account_2(100, u"nickname", GURL("http://www.example.com"),
                             bank_name_2, u"account_number_suffix",
                             BankAccount::AccountType::kChecking);

  // bank_account_1 is smaller than bank_account_2 because bank_name_1 <
  // bank_name_2, thus expect the output to be -1.
  EXPECT_EQ(-1, bank_account_1.Compare(bank_account_2));
}

TEST(BankAccountTest, Compare_BankNameGreater) {
  std::u16string bank_name_1 = u"bank_name_2";
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             bank_name_1, u"account_number_suffix",
                             BankAccount::AccountType::kChecking);
  std::u16string bank_name_2 = u"bank_name_1";
  BankAccount bank_account_2(100, u"nickname", GURL("http://www.example.com"),
                             bank_name_2, u"account_number_suffix",
                             BankAccount::AccountType::kChecking);

  // bank_account_1 is greater than bank_account_2 because bank_name_2 >
  // bank_name_1, thus expect the output to be 1.
  EXPECT_EQ(1, bank_account_1.Compare(bank_account_2));
}

TEST(BankAccountTest, Compare_AccountNumberSuffixSmaller) {
  std::u16string account_number_suffix_1 = u"account_number_suffix_1";
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", account_number_suffix_1,
                             BankAccount::AccountType::kChecking);
  std::u16string account_number_suffix_2 = u"account_number_suffix_2";
  BankAccount bank_account_2(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", account_number_suffix_2,
                             BankAccount::AccountType::kChecking);

  // bank_account_1 is smaller than bank_account_2 because
  // account_number_suffix_1 < account_number_suffix_2, thus expect the output
  // to be -1.
  EXPECT_EQ(-1, bank_account_1.Compare(bank_account_2));
}

TEST(BankAccountTest, Compare_AccountNumberSuffixGreater) {
  std::u16string account_number_suffix_1 = u"account_number_suffix_2";
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", account_number_suffix_1,
                             BankAccount::AccountType::kChecking);
  std::u16string account_number_suffix_2 = u"account_number_suffix_1";
  BankAccount bank_account_2(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", account_number_suffix_2,
                             BankAccount::AccountType::kChecking);

  // bank_account_1 is greater than bank_account_2 because
  // account_number_suffix_2 > account_number_suffix_1, thus expect the output
  // to be 1.
  EXPECT_EQ(1, bank_account_1.Compare(bank_account_2));
}

TEST(BankAccountTest, Compare_AccountTypeGreater) {
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kTransactingAccount);
  BankAccount bank_account_2(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kSalary);

  // bank_account_1 is greater than bank_account_2 because kTransactingAccount >
  // kSalary, thus expect the output to be 1.
  EXPECT_EQ(1, bank_account_1.Compare(bank_account_2));
}

TEST(BankAccountTest, Compare_AccountTypeSmaller) {
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kChecking);
  BankAccount bank_account_2(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kSalary);

  // bank_account_1 is smaller than bank_account_2 because kChecking <
  // kSalary, thus expect the output to be -1.
  EXPECT_EQ(-1, bank_account_1.Compare(bank_account_2));
}

TEST(BankAccountTest, Compare_IdenticalBankAccounts) {
  BankAccount bank_account_1(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kTransactingAccount);
  BankAccount bank_account_2(100, u"nickname", GURL("http://www.example.com"),
                             u"bank_name", u"account_number_suffix",
                             BankAccount::AccountType::kTransactingAccount);

  EXPECT_EQ(0, bank_account_1.Compare(bank_account_2));
}

}  // namespace autofill
