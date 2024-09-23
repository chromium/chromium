// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_ACCOUNT_ID_UTIL_H_
#define COMPONENTS_USER_MANAGER_ACCOUNT_ID_UTIL_H_

#include <optional>

#include "base/values.h"
#include "components/user_manager/user_manager_export.h"

class AccountId;

// Methods for serializing and de-serializing `AccountId` as a number of fields
// in `Dict`.
namespace user_manager {

// Fields used to serialize/deserialize AccountId.
// Note that this is an implementation detail of the user_manager.

// Key of canonical e-mail value.
USER_MANAGER_EXPORT extern const char kCanonicalEmail[];
// Key of obfuscated GAIA id value.
USER_MANAGER_EXPORT extern const char kGAIAIdKey[];
// Key of obfuscated object guid value for Active Directory accounts.
USER_MANAGER_EXPORT extern const char kObjGuidKey[];
// Key of account type.
USER_MANAGER_EXPORT extern const char kAccountTypeKey[];

// Attempts to construct `AccountId` based on provided `dict`.
// Resulting `AccountId` is not guaranteed to be fully resolved.
USER_MANAGER_EXPORT std::optional<AccountId> LoadAccountId(
    const base::Value::Dict& dict);

// Returns true if `account_id` matches the data in the dict.
// Note that match by id takes precedence over matching by e-mail.
USER_MANAGER_EXPORT bool AccountIdMatches(const AccountId& account_id,
                                          const base::Value::Dict& dict);

// Stores data relevant to `account_id` to `dict`.
USER_MANAGER_EXPORT void StoreAccountId(const AccountId& account_id,
                                        base::Value::Dict& dict);

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_ACCOUNT_ID_UTIL_H_
