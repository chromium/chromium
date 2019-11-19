// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

// This interface allows the caller to save passwords and blacklist entries in
// a password store.
class FormSaver {
 public:
  FormSaver() = default;

  virtual ~FormSaver() = default;

  // Blacklist the origin described by |digest|. Returns the PasswordForm pushed
  // to the store.
  virtual autofill::PasswordForm PermanentlyBlacklist(
      PasswordStore::FormDigest digest) = 0;

  // Unblacklist the origin described by |digest| by deleting all corresponding
  // blacklisted entries.
  virtual void Unblacklist(const PasswordStore::FormDigest& digest) = 0;

  // Saves the |pending| form.
  // |matches| are relevant credentials for the site. After saving |pending|,
  // the following clean up steps are performed on the credentials stored on
  // disk that correspond to |matches|:
  // - the |preferred| state is reset to false.
  // - empty-username credentials with the same password are removed.
  // - if |old_password| is provided, the old credentials with the same username
  //   and the old password are updated to the new password.
  virtual void Save(autofill::PasswordForm pending,
                    const std::vector<const autofill::PasswordForm*>& matches,
                    const base::string16& old_password) = 0;

  // Updates the saved credential in the password store sharing the same key as
  // the |pending| form.
  // The algorithm for handling |matches| and |old_password| is the same as
  // above.
  virtual void Update(autofill::PasswordForm pending,
                      const std::vector<const autofill::PasswordForm*>& matches,
                      const base::string16& old_password) = 0;

  // If any of the primary key fields (signon_realm, origin, username_element,
  // username_value, password_element) are updated, then the this version of
  // the Update method must be used, which takes |old_primary_key|, i.e., the
  // old values for the primary key fields (the rest of the fields are ignored).
  // The algorithm for handling |matches| and |old_password| is the same as
  // above.
  virtual void UpdateReplace(
      autofill::PasswordForm pending,
      const std::vector<const autofill::PasswordForm*>& matches,
      const base::string16& old_password,
      const autofill::PasswordForm& old_unique_key) = 0;

  // Removes |form| from the password store.
  virtual void Remove(const autofill::PasswordForm& form) = 0;

  // Creates a new FormSaver with the same state as |*this|.
  virtual std::unique_ptr<FormSaver> Clone() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormSaver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
