// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"

namespace autofill {

BankAccount::BankAccount(const BankAccount& other) = default;

BankAccount::BankAccount(int64_t instrument_id,
                         Nickname nickname,
                         const GURL& display_icon_url,
                         BankName bank_name,
                         AccountNumberSuffix account_number_suffix,
                         AccountType account_type)
    : PaymentInstrument(instrument_id, nickname, display_icon_url),
      bank_name_(bank_name),
      account_number_suffix_(account_number_suffix),
      account_type_(account_type) {}

BankAccount::~BankAccount() = default;

PaymentInstrument::InstrumentType BankAccount::GetInstrumentType() const {
  return PaymentInstrument::InstrumentType::kBankAccount;
}

bool BankAccount::AddToDatabase(AutofillTable* database) {
  return database->AddBankAccount(*this);
}

bool BankAccount::UpdateInDatabase(AutofillTable* database) {
  return database->UpdateBankAccount(*this);
}

bool BankAccount::DeleteFromDatabase(AutofillTable* database) {
  return database->RemoveBankAccount(instrument_id());
}

}  // namespace autofill
