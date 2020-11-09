// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_util.h"

#include "components/account_manager_core/account.h"

namespace account_manager {

base::Optional<account_manager::Account> FromMojoAccount(
    const crosapi::mojom::AccountPtr& mojom_account) {
  const base::Optional<account_manager::AccountType> account_type =
      FromMojoAccountType(mojom_account->key->account_type);
  if (!account_type.has_value())
    return base::nullopt;

  account_manager::Account account;
  account.key.id = mojom_account->key->id;
  account.key.account_type = account_type.value();
  account.raw_email = mojom_account->raw_email;
  return account;
}

crosapi::mojom::AccountPtr ToMojoAccount(
    const account_manager::Account& account) {
  crosapi::mojom::AccountPtr mojom_account = crosapi::mojom::Account::New();

  mojom_account->key = crosapi::mojom::AccountKey::New();
  mojom_account->key->id = account.key.id;
  mojom_account->key->account_type =
      ToMojoAccountType(account.key.account_type);
  mojom_account->raw_email = account.raw_email;

  return mojom_account;
}

base::Optional<account_manager::AccountType> FromMojoAccountType(
    const crosapi::mojom::AccountType& account_type) {
  switch (account_type) {
    case crosapi::mojom::AccountType::kGaia:
      static_assert(static_cast<int>(crosapi::mojom::AccountType::kGaia) ==
                        static_cast<int>(account_manager::AccountType::kGaia),
                    "Underlying enum values must match");
      return account_manager::AccountType::kGaia;
    case crosapi::mojom::AccountType::kActiveDirectory:
      static_assert(
          static_cast<int>(crosapi::mojom::AccountType::kActiveDirectory) ==
              static_cast<int>(account_manager::AccountType::kActiveDirectory),
          "Underlying enum values must match");
      return account_manager::AccountType::kActiveDirectory;
    default:
      // Don't consider this as as error to preserve forwards compatibility with
      // lacros.
      LOG(WARNING) << "Unknown account type: " << account_type;
      return base::nullopt;
  }
}

crosapi::mojom::AccountType ToMojoAccountType(
    const account_manager::AccountType& account_type) {
  switch (account_type) {
    case account_manager::AccountType::kGaia:
      return crosapi::mojom::AccountType::kGaia;
    case account_manager::AccountType::kActiveDirectory:
      return crosapi::mojom::AccountType::kActiveDirectory;
  }
}

}  // namespace account_manager
