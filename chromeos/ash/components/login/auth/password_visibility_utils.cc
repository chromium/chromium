// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/password_visibility_utils.h"

#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash::password_visibility {

bool AccountHasUserFacingPassword(PrefService* local_state,
                                  const AccountId& account_id) {
  // TODO(emaxx): Maintain this bit as more cases (e.g. Smart Cards) arise or
  // if/when the logic for determining accounts without a user facing password
  // is refined to reduce false negatives.
  return !user_manager::KnownUser(local_state)
              .GetIsUsingSAMLPrincipalsAPI(account_id);
}

}  // namespace ash::password_visibility
