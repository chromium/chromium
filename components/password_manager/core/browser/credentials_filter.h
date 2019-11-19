// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_FILTER_H_

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

class PasswordFormManager;

// This interface is used to filter credentials during saving, retrieval from
// PasswordStore, etc.
class CredentialsFilter {
 public:
  CredentialsFilter() {}
  virtual ~CredentialsFilter() {}

  // Should |form| be offered to be saved?
  virtual bool ShouldSave(const autofill::PasswordForm& form) const = 0;

  // Returns true if the hash of the password in |form| should be saved for Gaia
  // password reuse checking.
  virtual bool ShouldSaveGaiaPasswordHash(
      const autofill::PasswordForm& form) const = 0;

  // Returns true if the hash of the password in |form| should be saved for
  // enterprise password reuse checking.
  virtual bool ShouldSaveEnterprisePasswordHash(
      const autofill::PasswordForm& form) const = 0;

  // Call this if the form associated with |form_manager| was filled, and the
  // subsequent sign-in looked like a success.
  virtual void ReportFormLoginSuccess(
      const PasswordFormManager& form_manager) const {}

  // If |username| matches Chrome sync account email. For incognito profile,
  // it matches |username| against the sync account email used in its original
  // profile.
  virtual bool IsSyncAccountEmail(const std::string& username) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialsFilter);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_FILTER_H_
