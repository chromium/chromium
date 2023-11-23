// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_UTIL_H_

#include <vector>

#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

namespace password_manager {

// Aggregates a vector of PasswordChangesOrError into a single
// PasswordChangesOrError. Does not check for duplicate values.
// Will return first occurred error if any.
PasswordChanges JoinPasswordStoreChanges(
    const std::vector<PasswordChangesOrError>& changes);

// Returns logins if |result| holds them, or an empty list if |result|
// holds an error.
LoginsResult GetLoginsOrEmptyListOnFailure(LoginsResultOrError result);

// Returns password changes if |result| holds them, or std::nullopt if |result|
// holds an std::nullopt or error.
PasswordChanges GetPasswordChangesOrNulloptOnFailure(
    PasswordChangesOrError result);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_UTIL_H_
