// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_change.h"

namespace password_manager {

PasswordStoreChange::PasswordStoreChange(Type type, PasswordForm form)
    : type_(type), form_(std::move(form)) {}

PasswordStoreChange::PasswordStoreChange(
    Type type,
    PasswordForm form,
    bool password_changed,
    InsecureCredentialsChanged insecure_changed)
    : type_(type),
      form_(std::move(form)),
      password_changed_(password_changed),
      insecure_credentials_changed_(insecure_changed) {}

PasswordStoreChange::PasswordStoreChange(const PasswordStoreChange& other) =
    default;
PasswordStoreChange::PasswordStoreChange(PasswordStoreChange&& other) = default;
PasswordStoreChange& PasswordStoreChange::operator=(
    const PasswordStoreChange& change) = default;
PasswordStoreChange& PasswordStoreChange::operator=(
    PasswordStoreChange&& change) = default;
PasswordStoreChange::~PasswordStoreChange() = default;

bool PasswordStoreChange::operator==(const PasswordStoreChange& other) const {
  return type() == other.type() &&
         form().signon_realm == other.form().signon_realm &&
         form().url == other.form().url &&
         form().action == other.form().action &&
         form().submit_element == other.form().submit_element &&
         form().username_element == other.form().username_element &&
         form().username_value == other.form().username_value &&
         form().password_element == other.form().password_element &&
         form().password_value == other.form().password_value &&
         form().new_password_element == other.form().new_password_element &&
         form().new_password_value == other.form().new_password_value &&
         form().date_last_used == other.form().date_last_used &&
         form().date_created == other.form().date_created &&
         form().blocked_by_user == other.form().blocked_by_user &&
         form().password_issues == other.form().password_issues;
}

}  // namespace password_manager
