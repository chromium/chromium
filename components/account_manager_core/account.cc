// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account.h"

namespace account_manager {

bool AccountKey::IsValid() const {
  return !id.empty();
}

bool AccountKey::operator<(const AccountKey& other) const {
  if (id != other.id) {
    return id < other.id;
  }

  return account_type < other.account_type;
}

bool AccountKey::operator==(const AccountKey& other) const {
  return id == other.id && account_type == other.account_type;
}

bool AccountKey::operator!=(const AccountKey& other) const {
  return !(*this == other);
}

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountType& account_type) {
  switch (account_type) {
    case account_manager::AccountType::kGaia:
      os << "Gaia";
      break;
    case account_manager::AccountType::kActiveDirectory:
      os << "ActiveDirectory";
      break;
  }

  return os;
}

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountKey& account_key) {
  os << "{ id: " << account_key.id
     << ", account_type: " << account_key.account_type << " }";

  return os;
}

}  // namespace account_manager
