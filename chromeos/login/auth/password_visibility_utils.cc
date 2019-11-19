// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/password_visibility_utils.h"

#include "components/user_manager/known_user.h"

namespace chromeos {

namespace password_visibility {

bool AccountHasUserFacingPassword(const AccountId& account_id) {
  // TODO(emaxx): Maintain this bit as more cases (e.g. Smart Cards) arise or
  // if/when the logic for determining accounts without a user facing password
  // is refined to reduce false negatives.
  return !user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(account_id);
}

}  // namespace password_visibility

}  // namespace chromeos
