// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_FILTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_FILTER_H_

#include <string>

namespace password_manager {

struct PasswordForm;

// This interface is used to filter credentials during saving, retrieval from
// PasswordStore, etc.
class CredentialsFilter {
 public:
  CredentialsFilter() = default;
  CredentialsFilter(const CredentialsFilter&) = delete;
  CredentialsFilter& operator=(const CredentialsFilter&) = delete;
  virtual ~CredentialsFilter() = default;

  // Should |form| be offered to be saved?
  // Note that this only refers to *saving* - *updating* an already stored
  // credential should still be allowed even if this returns false!
  virtual bool ShouldSave(const PasswordForm& form) const = 0;

  // Returns true if the hash of the password in |form| should be saved for Gaia
  // password reuse checking.
  virtual bool ShouldSaveGaiaPasswordHash(const PasswordForm& form) const = 0;

  // Returns true if the hash of the password in |form| should be saved for
  // enterprise password reuse checking.
  virtual bool ShouldSaveEnterprisePasswordHash(
      const PasswordForm& form) const = 0;

  // If |username| matches Chrome sync account email. For incognito profile,
  // it matches |username| against the sync account email used in its original
  // profile.
  virtual bool IsSyncAccountEmail(const std::string& username) const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIALS_FILTER_H_
