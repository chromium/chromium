// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_UTIL_H_

#include <vector>

#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

// Aggregates a vector of PasswordStoreChangeLists into a single
// PasswordStoreChangeList. Does not check for duplicate values.
absl::optional<PasswordStoreChangeList> JoinPasswordStoreChanges(
    std::vector<absl::optional<PasswordStoreChangeList>> changes);

// Returns logins if |result| holds them, or an empty list if |result|
// holds an error.
LoginsResult GetLoginsOrEmptyListOnFailure(LoginsResultOrError result);

// Helper function allowing to bind base::OnceClosure to
// PasswordStoreChangeListReply.
PasswordStoreChangeListReply IgnoreChangeListAndRunCallback(
    base::OnceClosure callback);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_UTIL_H_
