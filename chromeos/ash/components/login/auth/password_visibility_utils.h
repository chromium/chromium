// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PASSWORD_VISIBILITY_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PASSWORD_VISIBILITY_UTILS_H_

#include "base/component_export.h"

class AccountId;
class PrefService;

namespace ash::password_visibility {

// Whether the account has a user facing password that the user can enter for
// security checks.
bool COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
    AccountHasUserFacingPassword(PrefService* local_state,
                                 const AccountId& account_id);

}  // namespace ash::password_visibility

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PASSWORD_VISIBILITY_UTILS_H_
