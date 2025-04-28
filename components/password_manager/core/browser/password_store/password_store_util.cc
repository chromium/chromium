// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_util.h"

#include <algorithm>
#include <variant>

namespace password_manager {

PasswordChanges JoinPasswordStoreChanges(
    const std::vector<PasswordChangesOrError>& changes_to_join) {
  PasswordStoreChangeList joined_changes;
  for (const auto& changes_or_error : changes_to_join) {
    if (std::holds_alternative<PasswordStoreBackendError>(changes_or_error)) {
      return std::nullopt;
    }
    const PasswordChanges& changes =
        std::get<PasswordChanges>(changes_or_error);
    if (!changes.has_value()) {
      return std::nullopt;
    }
    std::ranges::copy(*changes, std::back_inserter(joined_changes));
  }
  return joined_changes;
}

LoginsResult GetLoginsOrEmptyListOnFailure(LoginsResultOrError result) {
  if (std::holds_alternative<PasswordStoreBackendError>(result)) {
    return {};
  }
  return std::move(std::get<LoginsResult>(result));
}

PasswordChanges GetPasswordChangesOrNulloptOnFailure(
    PasswordChangesOrError result) {
  if (std::holds_alternative<PasswordStoreBackendError>(result)) {
    return std::nullopt;
  }
  return std::move(std::get<PasswordChanges>(result));
}

std::vector<std::unique_ptr<PasswordForm>> ConvertPasswordToUniquePtr(
    std::vector<PasswordForm> forms) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.reserve(forms.size());
  for (auto& form : forms) {
    result.push_back(std::make_unique<PasswordForm>(std::move(form)));
  }
  return result;
}

}  // namespace password_manager
