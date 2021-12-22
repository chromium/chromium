// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_util.h"

namespace password_manager {

absl::optional<PasswordStoreChangeList> JoinPasswordStoreChanges(
    std::vector<absl::optional<PasswordStoreChangeList>> changes) {
  PasswordStoreChangeList joined_changes;
  for (auto changes_list : changes) {
    if (!changes_list.has_value())
      return absl::nullopt;
    std::move(changes_list->begin(), changes_list->end(),
              std::back_inserter(joined_changes));
  }
  return joined_changes;
}

LoginsResult GetLoginsOrEmptyListOnFailure(LoginsResultOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    return {};
  }
  return std::move(absl::get<LoginsResult>(result));
}

PasswordStoreChangeListReply IgnoreChangeListAndRunCallback(
    base::OnceClosure callback) {
  return base::BindOnce(
      [](base::OnceClosure callback, absl::optional<PasswordStoreChangeList>) {
        std::move(callback).Run();
      },
      std::move(callback));
}

}  // namespace password_manager
