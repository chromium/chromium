// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payments/bank_account.h"

#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(BankAccountTest, VerifyAllFields) {
  BankAccount bank_account(100, u"nickname", GURL("http://www.example.com"),
                           u"bank_name", u"account_number_suffix",
                           BankAccount::AccountType::kChecking);

  EXPECT_EQ(bank_account.payment_instrument().instrument_id(), 100);
  EXPECT_EQ(bank_account.bank_name(), u"bank_name");
  EXPECT_EQ(bank_account.account_number_suffix(), u"account_number_suffix");
  EXPECT_EQ(bank_account.payment_instrument().nickname(), u"nickname");
  EXPECT_EQ(GURL("http://www.example.com"),
            bank_account.payment_instrument().display_icon_url());
  EXPECT_EQ(BankAccount::AccountType::kChecking, bank_account.account_type());
  EXPECT_TRUE(bank_account.payment_instrument().IsSupported(
      PaymentInstrument::PaymentRail::kPix));
}

}  // namespace autofill
