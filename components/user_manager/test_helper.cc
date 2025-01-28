// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/test_helper.h"

#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager_pref_names.h"

namespace user_manager {

// static
void TestHelper::RegisterPersistedUser(PrefService& local_state,
                                       const AccountId& account_id) {
  {
    ScopedListPrefUpdate update(&local_state, prefs::kRegularUsersPref);
    update->Append(account_id.GetUserEmail());
  }
  {
    KnownUser known_user(&local_state);
    known_user.UpdateId(account_id);
  }
}

}  // namespace user_manager
