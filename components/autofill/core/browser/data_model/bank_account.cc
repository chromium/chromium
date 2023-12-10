// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"

namespace autofill {

bool operator==(const BankAccount& a, const BankAccount& b) {
  return (static_cast<const PaymentInstrument&>(a) ==
          static_cast<const PaymentInstrument&>(b)) &&
         a.bank_name() == b.bank_name() &&
         a.account_number_suffix() == b.account_number_suffix() &&
         a.account_type() == b.account_type();
}

BankAccount::BankAccount(const BankAccount& other) = default;
BankAccount& BankAccount::operator=(const BankAccount& other) = default;

BankAccount::BankAccount(int64_t instrument_id,
                         std::u16string_view nickname,
                         const GURL& display_icon_url,
                         std::u16string_view bank_name,
                         std::u16string_view account_number_suffix,
                         AccountType account_type)
    : PaymentInstrument(instrument_id, nickname, display_icon_url),
      bank_name_(bank_name),
      account_number_suffix_(account_number_suffix),
      account_type_(account_type) {}

BankAccount::~BankAccount() = default;

PaymentInstrument::InstrumentType BankAccount::GetInstrumentType() const {
  return PaymentInstrument::InstrumentType::kBankAccount;
}

bool BankAccount::AddToDatabase(AutofillTable* database) const {
  return database->AddBankAccount(*this);
}

bool BankAccount::UpdateInDatabase(AutofillTable* database) const {
  return database->UpdateBankAccount(*this);
}

bool BankAccount::DeleteFromDatabase(AutofillTable* database) const {
  return database->RemoveBankAccount(*this);
}

}  // namespace autofill
