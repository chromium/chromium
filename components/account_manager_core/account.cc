// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account.h"

#include "base/check.h"

namespace account_manager {

AccountKey::AccountKey(const std::string& id, AccountType type)
    : id_(id), account_type_(type) {
  DCHECK(!id_.empty());
}

bool AccountKey::operator<(const AccountKey& other) const {
  return std::tie(id_, account_type_) <
         std::tie(other.id_, other.account_type_);
}

bool AccountKey::operator==(const AccountKey& other) const {
  return std::tie(id_, account_type_) ==
         std::tie(other.id_, other.account_type_);
}

bool AccountKey::operator!=(const AccountKey& other) const {
  return !(*this == other);
}

bool Account::operator<(const Account& other) const {
  return std::tie(key, raw_email) < std::tie(other.key, other.raw_email);
}

bool Account::operator==(const Account& other) const {
  return std::tie(key, raw_email) == std::tie(other.key, other.raw_email);
}

bool Account::operator!=(const Account& other) const {
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
  os << "{ id: " << account_key.id()
     << ", account_type: " << account_key.account_type() << " }";

  return os;
}

}  // namespace account_manager
