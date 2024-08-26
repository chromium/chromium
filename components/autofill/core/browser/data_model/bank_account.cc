// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bank_account.h"

#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"

namespace autofill {

std::strong_ordering operator<=>(const BankAccount&,
                                 const BankAccount&) = default;
bool operator==(const BankAccount&, const BankAccount&) = default;

BankAccount::BankAccount(const BankAccount& other) = default;
BankAccount& BankAccount::operator=(const BankAccount& other) = default;

BankAccount::BankAccount(int64_t instrument_id,
                         std::u16string nickname,
                         GURL display_icon_url,
                         std::u16string bank_name,
                         std::u16string account_number_suffix,
                         AccountType account_type)
    : bank_name_(std::move(bank_name)),
      account_number_suffix_(std::move(account_number_suffix)),
      account_type_(account_type),
      payment_instrument_(instrument_id,
                          std::move(nickname),
                          std::move(display_icon_url),
                          DenseSet({PaymentInstrument::PaymentRail::kPix})) {}

BankAccount::~BankAccount() = default;

}  // namespace autofill
