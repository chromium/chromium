// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/components/account_manager/tokens.pb.h"

namespace account_manager {

struct COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountKey {
  // |id| is obfuscated GAIA id for |AccountType::ACCOUNT_TYPE_GAIA|.
  // |id| is object GUID (|AccountId::GetObjGuid|) for
  // |AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY|.
  std::string id;
  // TODO(sinhak): Remove dependency on chromeos::account_manager::AccountType
  // by creating a new standalone AccountType enum.
  chromeos::account_manager::AccountType account_type;

  bool IsValid() const;

  bool operator<(const AccountKey& other) const;
  bool operator==(const AccountKey& other) const;
  bool operator!=(const AccountKey& other) const;
};

// Publicly viewable information about an account.
struct COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) Account {
  // A unique identifier for this account.
  AccountKey key;

  // The raw, un-canonicalized email id for this account.
  std::string raw_email;
};

// For logging.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
std::ostream& operator<<(std::ostream& os, const AccountKey& account_key);

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_H_
