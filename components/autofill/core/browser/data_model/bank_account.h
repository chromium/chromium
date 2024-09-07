// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BANK_ACCOUNT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BANK_ACCOUNT_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/data_model/payment_instrument.h"

class GURL;

namespace autofill {

// Details for a user's bank account. This data is synced from Google payments.
class BankAccount {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill.payments
  // The type of bank account owned by the user. This is used for display
  // purposes only.
  enum class AccountType {
    kUnknown = 0,
    kChecking = 1,
    kSavings = 2,
    kCurrent = 3,
    kSalary = 4,
    kTransactingAccount = 5
  };

  BankAccount(const BankAccount& other);
  BankAccount& operator=(const BankAccount& other);
  BankAccount(int64_t instrument_id,
              std::u16string nickname,
              GURL display_icon_url,
              std::u16string bank_name,
              std::u16string account_number_suffix,
              AccountType account_type);
  ~BankAccount();

  friend std::strong_ordering operator<=>(const BankAccount&,
                                          const BankAccount&);
  friend bool operator==(const BankAccount&, const BankAccount&);

  const PaymentInstrument& payment_instrument() const {
    return payment_instrument_;
  }

  const std::u16string& bank_name() const { return bank_name_; }

  const std::u16string& account_number_suffix() const {
    return account_number_suffix_;
  }

  AccountType account_type() const { return account_type_; }

 private:
  // The name of the bank to which the account belongs to. This is not
  // localized.
  std::u16string bank_name_;

  // The account number suffix used to identify the bank account.
  std::u16string account_number_suffix_;

  // The type of bank account.
  AccountType account_type_ = AccountType::kUnknown;

  // Fields common for all types of payment instruments.
  PaymentInstrument payment_instrument_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BANK_ACCOUNT_H_
