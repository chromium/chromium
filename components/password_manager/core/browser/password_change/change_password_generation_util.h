// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_CHANGE_PASSWORD_GENERATION_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_CHANGE_PASSWORD_GENERATION_UTIL_H_

#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/password_manager/core/browser/password_string.h"

namespace password_manager {

struct PasswordForm;
class PasswordGenerationFrameHelper;

// The result of generating a password for a change password form.
struct ChangePasswordGenerationResult {
  // The generated password.
  PasswordString password;
  // The password requirements the password was generated for.
  autofill::PasswordRequirementsSpec spec;
};

// Generates a new password for the change password `form`, respecting the
// crowdsourced password requirements for its new password field or, if there
// is none, for its current password field. If `form` has neither, only the
// domain-wide requirements are respected.
ChangePasswordGenerationResult GeneratePasswordForChangePasswordForm(
    const PasswordForm& form,
    PasswordGenerationFrameHelper& generation_helper);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_CHANGE_PASSWORD_GENERATION_UTIL_H_
