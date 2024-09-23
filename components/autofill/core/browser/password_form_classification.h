// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_FORM_CLASSIFICATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_FORM_CLASSIFICATION_H_

#include <optional>

#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// `PasswordFormClassification` describes the different outcomes of Password
// Manager's form parsing heuristics (see `FormDataParser`). Note that these
// are all predictions and may be inaccurate.
struct PasswordFormClassification {
  bool operator==(const PasswordFormClassification&) const = default;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Type {
    // The form is not password-related.
    kNoPasswordForm = 0,
    // The form is a predicted to be a login form, i.e. it has a username and
    // a
    // password field.
    kLoginForm = 1,
    // The form is predicted to be a signup form, i.e. it has a username field
    // and a new password field.
    kSignupForm = 2,
    // The form is predicted to be a change password form, i.e. it has a
    // current
    // password field and a new password field.
    kChangePasswordForm = 3,
    // The form is predicted to be a reset password form, i.e. it has a new
    // password field.
    kResetPasswordForm = 4,
    // The form is predicted to be the username form of a username-first flow,
    // i.e. there is only a username field.
    kSingleUsernameForm = 5
  } type = Type::kNoPasswordForm;
  std::optional<FieldGlobalId> username_field;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_FORM_CLASSIFICATION_H_
