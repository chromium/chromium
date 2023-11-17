// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bank_account.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(BankAccountTest, VerifyAllFields) {
  BankAccount bank_account(100, u"nickname", GURL("http://www.example.com"),
                           u"bank_name", u"account_number_suffix",
                           BankAccount::AccountType::kChecking);

  EXPECT_EQ(100, bank_account.instrument_id());
  EXPECT_EQ(u"bank_name", bank_account.bank_name());
  EXPECT_EQ(u"account_number_suffix", bank_account.account_number_suffix());
  EXPECT_EQ(u"nickname", bank_account.nickname());
  EXPECT_EQ(GURL("http://www.example.com"), bank_account.display_icon_url());
  EXPECT_EQ(BankAccount::AccountType::kChecking, bank_account.account_type());
  EXPECT_EQ(PaymentInstrument::InstrumentType::kBankAccount,
            bank_account.GetInstrumentType());
}

}  // namespace autofill
