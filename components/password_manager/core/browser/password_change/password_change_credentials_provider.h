// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIALS_PROVIDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIALS_PROVIDER_H_

#include "base/types/expected.h"
#include "components/password_manager/core/browser/password_string.h"

namespace password_manager {

class PasswordFormManager;

// Provides the credentials needed to fill a change password form during an
// ongoing password change flow (e.g. by the change password actor tool).
class PasswordChangeCredentialsProvider {
 public:
  // Reasons why a password couldn't be provided.
  enum class Error {
    // No password change flow is running which could provide the password.
    kNoPasswordChangeFlow,
    // The new password couldn't be generated.
    kGenerationFailed,
  };

  virtual ~PasswordChangeCredentialsProvider() = default;

  // Returns the current (old) password which is being changed, or an error if
  // it isn't available.
  virtual base::expected<PasswordString, Error> GetOldPassword() const = 0;

  // Returns the new password which should replace the old one. If it doesn't
  // exist yet, it is generated for the change password form observed by
  // `form_manager`, respecting its crowdsourced password requirements, and
  // saved as a backup of the credential being changed, so it isn't lost if the
  // change succeeds on the website but isn't saved by Chrome. If
  // `form_manager` is null, only an already existing password is returned.
  // Returns an error if no new password could be provided; the returned
  // password is never empty.
  virtual base::expected<PasswordString, Error> GetNewPassword(
      PasswordFormManager* form_manager) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_PASSWORD_CHANGE_CREDENTIALS_PROVIDER_H_
