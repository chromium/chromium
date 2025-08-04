// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SERVICE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SERVICE_INTERFACE_H_

#include <vector>

#include "components/autofill/core/common/language_code.h"
#include "url/gurl.h"

namespace password_manager {

enum class LogInWithChangedPasswordOutcome;

// Abstract interface for high level interaction related to password change.
class PasswordChangeServiceInterface {
 public:
  // Checks whether current user is eligible to use password change.
  virtual bool IsPasswordChangeAvailable() const = 0;

  // Checks whether password change is eligible for a given `url` and
  // `page_language`.
  virtual bool IsPasswordChangeSupported(
      const GURL& url,
      const autofill::LanguageCode& page_language) const = 0;

  // Records the outcome of the first login attempt
  // using a previously saved APC-password and immediately
  // uploads it to the server.
  virtual void RecordLoginAttemptQuality(
      LogInWithChangedPasswordOutcome outcome,
      const GURL& page_url) const = 0;
};

// Return overridden change password URL passed to chrome switch.
std::vector<GURL> GetChangePasswordUrlOverrides();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_SERVICE_INTERFACE_H_
