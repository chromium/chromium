// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_SIGNIN_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_SIGNIN_TEST_UTILS_H_

#include "components/signin/public/identity_manager/account_info.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace supervised_user {

// Modifies the supervision status for an AccountInfo and updates the given
// IdentityManager, which should already have the account signed in.
void UpdateSupervisionStatusForAccount(
    AccountInfo& account,
    signin::IdentityManager* identity_manager,
    bool is_subject_to_parental_controls);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_SUPERVISED_USER_SIGNIN_TEST_UTILS_H_
