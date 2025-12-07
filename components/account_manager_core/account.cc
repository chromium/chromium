// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account.h"

#include "base/check.h"
#include "base/check_op.h"
#include "google_apis/gaia/gaia_id.h"

namespace account_manager {

// static
AccountKey AccountKey::FromGaiaId(const GaiaId& gaia_id) {
  return AccountKey(gaia_id.ToString(), AccountType::kGaia);
}

AccountKey::AccountKey(const std::string& id, AccountType type)
    : id_(id), account_type_(type) {
  DCHECK(!id_.empty());
}

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountType& account_type) {
  // Currently, we only support `kGaia` account type. Should a new type be added
  // in the future, consider removing the `CHECK_EQ()` below and handling the
  // new type accordingly.
  CHECK_EQ(account_type, account_manager::AccountType::kGaia);

  os << "Gaia";
  return os;
}

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountKey& account_key) {
  os << "{ id: " << account_key.id()
     << ", account_type: " << account_key.account_type() << " }";

  return os;
}

}  // namespace account_manager
