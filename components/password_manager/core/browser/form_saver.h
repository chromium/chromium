// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

struct PasswordForm;

// This interface allows the caller to save passwords and blocklist entries in
// a password store.
class FormSaver {
 public:
  FormSaver() = default;

  FormSaver(const FormSaver&) = delete;
  FormSaver& operator=(const FormSaver&) = delete;

  virtual ~FormSaver() = default;

  // Blocklist the origin described by |digest|. Returns the PasswordForm pushed
  // to the store.
  virtual PasswordForm Blocklist(PasswordFormDigest digest) = 0;

  // Unblocklist the origin described by |digest| by deleting all corresponding
  // blocklisted entries.
  virtual void Unblocklist(const PasswordFormDigest& digest) = 0;

  // Saves the |pending| form.
  // |matches| are relevant credentials for the site. After saving |pending|,
  // the following clean up steps are performed on the credentials stored on
  // disk that correspond to |matches|:
  // - empty-username credentials with the same password are removed.
  // - if |old_password| is provided, the old credentials with the same username
  //   and the old password are updated to the new password.
  virtual void Save(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password) = 0;

  // Updates the saved credential in the password store sharing the same key as
  // the |pending| form.
  // The algorithm for handling |matches| and |old_password| is the same as
  // above.
  virtual void Update(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password) = 0;

  // If any of the unique key fields (signon_realm, origin, username_element,
  // username_value, password_element) are updated, then the this version of
  // the Update method must be used, which takes |old_unique_key|, i.e., the
  // old values for the unique key fields (the rest of the fields are ignored).
  // The algorithm for handling |matches| and |old_password| is the same as
  // above.
  virtual void UpdateReplace(
      PasswordForm pending,
      const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
          matches,
      const std::u16string& old_password,
      const PasswordForm& old_unique_key) = 0;

  // Removes |form| from the password store.
  virtual void Remove(const PasswordForm& form) = 0;

  // Creates a new FormSaver with the same state as |*this|.
  virtual std::unique_ptr<FormSaver> Clone() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
