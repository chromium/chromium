// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_

#include <ostream>
#include <string>

#include "base/component_export.h"

namespace account_manager {

// Type of an account, based on the authentication backend of the account.
// Loosely based on //components/account_manager_core/chromeos/tokens.proto.
enum class AccountType : int {
  // Gaia account (aka Google account) - including enterprise and consumer
  // accounts.
  kGaia = 1,
  // Microsoft Active Directory accounts.
  kActiveDirectory = 2,
};

// Uniquely identifies an account.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountKey {
 public:
  // `id` cannot be empty.
  AccountKey(const std::string& id, AccountType type);

  // |id| is obfuscated GAIA id for |AccountType::kGaia|, and cannot be empty.
  // |id| is object GUID (|AccountId::GetObjGuid|) for
  // |AccountType::kActiveDirectory|.
  const std::string& id() const { return id_; }
  AccountType account_type() const { return account_type_; }

  bool operator<(const AccountKey& other) const;
  bool operator==(const AccountKey& other) const;
  bool operator!=(const AccountKey& other) const;

 private:
  // Fields are not const to allow assignment operator.
  std::string id_;
  AccountType account_type_;
};

// Publicly viewable information about an account.
struct COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) Account {
  // A unique identifier for this account.
  AccountKey key;

  // The raw, un-canonicalized email id for this account.
  std::string raw_email;

  bool operator<(const Account& other) const;
  bool operator==(const Account& other) const;
  bool operator!=(const Account& other) const;
};

// For logging.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountType& account_type);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountKey& account_key);

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_
