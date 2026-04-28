// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_change.h"

#include "components/password_manager/core/browser/password_store/password_form_converters.h"

namespace password_manager {

PasswordStoreChange::PasswordStoreChange(Type type, StoredCredential credential)
    : type_(type), credential_(std::move(credential)) {}

PasswordStoreChange::PasswordStoreChange(
    Type type,
    StoredCredential credential,
    bool password_changed,
    InsecureCredentialsChanged insecure_changed)
    : type_(type),
      credential_(std::move(credential)),
      password_changed_(password_changed),
      insecure_credentials_changed_(insecure_changed) {}

PasswordStoreChange::PasswordStoreChange(const PasswordStoreChange& other)
    : type_(other.type_),
      credential_(CloneStoredCredential(other.credential_)),
      password_changed_(other.password_changed_),
      insecure_credentials_changed_(other.insecure_credentials_changed_) {}

PasswordStoreChange::PasswordStoreChange(PasswordStoreChange&& other) = default;

PasswordStoreChange& PasswordStoreChange::operator=(
    const PasswordStoreChange& other) {
  if (this == &other) {
    return *this;
  }
  type_ = other.type_;
  credential_ = CloneStoredCredential(other.credential_);
  password_changed_ = other.password_changed_;
  insecure_credentials_changed_ = other.insecure_credentials_changed_;
  return *this;
}

PasswordStoreChange& PasswordStoreChange::operator=(
    PasswordStoreChange&& change) = default;

PasswordStoreChange::~PasswordStoreChange() = default;

bool PasswordStoreChange::operator==(const PasswordStoreChange& other) const {
  return type() == other.type() &&
         credential().signon_realm == other.credential().signon_realm &&
         credential().url == other.credential().url &&
         credential().action == other.credential().action &&
         credential().submit_element == other.credential().submit_element &&
         credential().username_element == other.credential().username_element &&
         credential().username_value == other.credential().username_value &&
         credential().password_element == other.credential().password_element &&
         credential().password_value == other.credential().password_value &&
         credential().date_last_used == other.credential().date_last_used &&
         credential().date_last_filled == other.credential().date_last_filled &&
         credential().date_created == other.credential().date_created &&
         credential().blocked_by_user == other.credential().blocked_by_user &&
         credential().password_issues == other.credential().password_issues &&
         password_changed() == other.password_changed() &&
         insecure_credentials_changed() == other.insecure_credentials_changed();
}

}  // namespace password_manager
