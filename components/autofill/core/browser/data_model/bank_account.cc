// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/bank_account.h"

#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"

namespace autofill {

bool operator==(const BankAccount&, const BankAccount&) = default;

BankAccount::BankAccount(const BankAccount& other) = default;
BankAccount& BankAccount::operator=(const BankAccount& other) = default;

BankAccount::BankAccount(int64_t instrument_id,
                         std::u16string_view nickname,
                         const GURL& display_icon_url,
                         std::u16string_view bank_name,
                         std::u16string_view account_number_suffix,
                         AccountType account_type)
    : bank_name_(bank_name),
      account_number_suffix_(account_number_suffix),
      account_type_(account_type),
      payment_instrument_(instrument_id,
                          nickname,
                          display_icon_url,
                          DenseSet<PaymentInstrument::PaymentRail>(
                              {PaymentInstrument::PaymentRail::kPix})) {}

BankAccount::~BankAccount() = default;

int BankAccount::Compare(const BankAccount& other) const {
  int comparison = payment_instrument_.Compare(other.payment_instrument());
  if (comparison != 0) {
    return comparison;
  }

  comparison = bank_name_.compare(other.bank_name());
  if (comparison < 0) {
    return -1;
  } else if (comparison > 0) {
    return 1;
  }

  comparison = account_number_suffix_.compare(other.account_number_suffix());
  if (comparison < 0) {
    return -1;
  } else if (comparison > 0) {
    return 1;
  }

  if (account_type_ < other.account_type()) {
    return -1;
  }

  if (account_type_ > other.account_type()) {
    return 1;
  }

  return 0;
}

}  // namespace autofill
