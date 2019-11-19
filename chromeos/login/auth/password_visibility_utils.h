// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_PASSWORD_VISIBILITY_UTILS_H_
#define CHROMEOS_LOGIN_AUTH_PASSWORD_VISIBILITY_UTILS_H_

#include "base/component_export.h"

class AccountId;

namespace chromeos {

namespace password_visibility {

// Whether the account has a user facing password that the user can enter for
// security checks.
bool COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
    AccountHasUserFacingPassword(const AccountId& account_id);

}  // namespace password_visibility

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_PASSWORD_VISIBILITY_UTILS_H_
