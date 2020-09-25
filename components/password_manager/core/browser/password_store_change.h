// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H__
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H__

#include <ostream>
#include <vector>

#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

class PasswordStoreChange {
 public:
  // This is used in enums.xml. Please keep order the same.
  enum Type { ADD = 0, UPDATE = 1, REMOVE = 2, kMaxValue = REMOVE };

  // TODO(crbug.com/902349): The following constructor is important only in
  // Linux backends production. It should be available only on Linux, and all
  // test code should be updates to the other constructor that accepts a
  // |primary_key|.
  PasswordStoreChange(Type type, PasswordForm form)
      : type_(type), form_(std::move(form)) {}
  PasswordStoreChange(Type type, PasswordForm form, int primary_key)
      : type_(type), form_(std::move(form)), primary_key_(primary_key) {}
  PasswordStoreChange(Type type,
                      PasswordForm form,
                      int primary_key,
                      bool password_changed)
      : type_(type),
        form_(std::move(form)),
        primary_key_(primary_key),
        password_changed_(password_changed) {}
  PasswordStoreChange(const PasswordStoreChange& other) = default;
  PasswordStoreChange(PasswordStoreChange&& other) = default;
  PasswordStoreChange& operator=(const PasswordStoreChange& change) = default;
  PasswordStoreChange& operator=(PasswordStoreChange&& change) = default;
  virtual ~PasswordStoreChange() {}

  Type type() const { return type_; }
  const PasswordForm& form() const { return form_; }
  int primary_key() const { return primary_key_; }
  bool password_changed() const { return password_changed_; }

  bool operator==(const PasswordStoreChange& other) const {
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
           form().blocked_by_user == other.form().blocked_by_user;
  }

 private:
  Type type_;
  PasswordForm form_;
  // The corresponding primary key in the database for this password.
  int primary_key_ = -1;
  bool password_changed_ = false;
};

typedef std::vector<PasswordStoreChange> PasswordStoreChangeList;

// For testing.
std::ostream& operator<<(std::ostream& os,
                         const PasswordStoreChange& password_store_change);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_CHANGE_H_
