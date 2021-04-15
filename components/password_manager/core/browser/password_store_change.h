// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H__
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H__

#include <ostream>
#include <vector>

#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

using InsecureCredentialsChanged =
    base::StrongAlias<class InsecureCredentialsChangedTag, bool>;

class PasswordStoreChange {
 public:
  // This is used in enums.xml. Please keep order the same.
  enum Type { ADD = 0, UPDATE = 1, REMOVE = 2, kMaxValue = REMOVE };

  // TODO(crbug.com/902349): The following constructor is important only in
  // Linux backends production. It should be available only on Linux, and all
  // test code should be updates to the other constructor that accepts a
  // |primary_key|.
  PasswordStoreChange(Type type, PasswordForm form);
  PasswordStoreChange(Type type, PasswordForm form, FormPrimaryKey primary_key);
  PasswordStoreChange(Type type,
                      PasswordForm form,
                      FormPrimaryKey primary_key,
                      bool password_changed,
                      InsecureCredentialsChanged insecure_changed =
                          InsecureCredentialsChanged(false));

  PasswordStoreChange(const PasswordStoreChange& other);
  PasswordStoreChange(PasswordStoreChange&& other);
  PasswordStoreChange& operator=(const PasswordStoreChange& change);
  PasswordStoreChange& operator=(PasswordStoreChange&& change);
  ~PasswordStoreChange();

  Type type() const { return type_; }
  const PasswordForm& form() const { return form_; }
  FormPrimaryKey primary_key() const { return primary_key_; }
  bool password_changed() const { return password_changed_; }
  InsecureCredentialsChanged insecure_credentials_changed() const {
    return insecure_credentials_changed_;
  }

  bool operator==(const PasswordStoreChange& other) const;

 private:
  Type type_;
  PasswordForm form_;
  // The corresponding primary key in the database for this password.
  FormPrimaryKey primary_key_{-1};
  bool password_changed_ = false;
  // Whether change affected insecure credentials.
  InsecureCredentialsChanged insecure_credentials_changed_{false};
};

typedef std::vector<PasswordStoreChange> PasswordStoreChangeList;

// For testing.
#if defined(UNIT_TEST)
inline std::ostream& operator<<(
    std::ostream& os,
    const PasswordStoreChange& password_store_change) {
  return os << "type: " << password_store_change.type()
            << ", primary key: " << password_store_change.primary_key()
            << ", password change: " << password_store_change.password_changed()
            << ", password form: " << password_store_change.form();
}
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H_
