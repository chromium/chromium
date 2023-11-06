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
class BankAccount : public PaymentInstrument {
 public:
  // The type of bank account owned by the user. This is used for display
  // purposes only.
  enum class AccountType {
    kAccountTypeUnspecified = 0,
    kChecking = 1,
    kSavings = 2,
    kCurrent = 3,
    kSalary = 4,
    kTransactingAccount = 5
  };

  using BankName = base::StrongAlias<class BankNameTag, std::u16string_view>;
  using AccountNumberSuffix =
      base::StrongAlias<class AccountNumberSuffixTag, std::u16string_view>;

  BankAccount(const BankAccount& other);
  BankAccount(int64_t instrument_id,
              Nickname nickname,
              const GURL& display_icon_url,
              BankName bank_name,
              AccountNumberSuffix account_number_suffix,
              AccountType account_type);
  ~BankAccount() override;

  // PaymentInstrument
  PaymentInstrument::InstrumentType GetInstrumentType() const override;
  bool AddToDatabase(AutofillTable* database) override;
  bool UpdateInDatabase(AutofillTable* database) override;
  bool DeleteFromDatabase(AutofillTable* database) override;

  BankName bank_name() const { return bank_name_; }
  void set_bank_name(BankName bank_name) { bank_name_ = bank_name; }

  AccountNumberSuffix account_number_suffix() const {
    return account_number_suffix_;
  }
  void set_account_number_suffix(AccountNumberSuffix account_number_suffix) {
    account_number_suffix_ = account_number_suffix;
  }

  AccountType account_type() const { return account_type_; }
  void set_account_type(AccountType account_type) {
    account_type_ = account_type;
  }

 private:
  // The name of the bank to which the account belongs to. This is not
  // localized.
  BankName bank_name_;

  // The account number suffix used to identify the bank account.
  AccountNumberSuffix account_number_suffix_;

  // The type of bank account.
  AccountType account_type_ = AccountType::kAccountTypeUnspecified;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BANK_ACCOUNT_H_
