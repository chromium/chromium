// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CHANGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CHANGE_H_

#include <optional>
#include <ostream>
#include <vector>

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"

namespace password_manager {

using InsecureCredentialsChanged =
    base::StrongAlias<class InsecureCredentialsChangedTag, bool>;

class PasswordStoreChange {
 public:
  // This is used in enums.xml. Please keep order the same.
  enum Type { ADD = 0, UPDATE = 1, REMOVE = 2, kMaxValue = REMOVE };

  PasswordStoreChange(Type type, PasswordForm form);
  PasswordStoreChange(Type type,
                      PasswordForm form,
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
  bool password_changed() const { return password_changed_; }
  InsecureCredentialsChanged insecure_credentials_changed() const {
    return insecure_credentials_changed_;
  }

  bool operator==(const PasswordStoreChange& other) const;

 private:
  Type type_;
  PasswordForm form_;
  bool password_changed_ = false;
  // Whether change affected insecure credentials.
  InsecureCredentialsChanged insecure_credentials_changed_{false};
};

using PasswordStoreChangeList = std::vector<PasswordStoreChange>;
using PasswordChanges = std::optional<PasswordStoreChangeList>;
using PasswordChangesOrError =
    absl::variant<PasswordChanges, PasswordStoreBackendError>;

// For testing.
#if defined(UNIT_TEST)
inline std::ostream& operator<<(
    std::ostream& os,
    const PasswordStoreChange& password_store_change) {
  return os << "type: " << password_store_change.type()
            << ", password change: " << password_store_change.password_changed()
            << ", password form: " << password_store_change.form();
}
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_CHANGE_H_
