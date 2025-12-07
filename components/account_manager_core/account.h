// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_

#include <ostream>
#include <string>

#include "base/component_export.h"

class GaiaId;

namespace account_manager {

// Type of an account, based on the authentication backend of the account.
// Loosely based on //components/account_manager_core/chromeos/tokens.proto.
enum class AccountType : int {
  // Gaia account (aka Google account) - including enterprise and consumer
  // accounts.
  kGaia = 1,
  // Value 2 was used for the deprecated `kActiveDirectory` account type.
};

// Uniquely identifies an account.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountKey {
 public:
  // Convenience factory function to create an instance for
  // `AccountType::kGaia`.
  static AccountKey FromGaiaId(const GaiaId& gaia_id);

  // `id` cannot be empty.
  AccountKey(const std::string& id, AccountType type);

  // `id` is obfuscated GAIA id for `AccountType::kGaia`, and cannot be empty.
  const std::string& id() const { return id_; }
  AccountType account_type() const { return account_type_; }

  friend bool operator==(const AccountKey&, const AccountKey&) = default;
  friend auto operator<=>(const AccountKey&, const AccountKey&) = default;

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

  friend bool operator==(const Account&, const Account&) = default;
  friend auto operator<=>(const Account&, const Account&) = default;
};

// For logging.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountType& account_type);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountKey& account_key);

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_
