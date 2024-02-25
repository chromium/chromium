// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"

#include "base/notreached.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

using password_manager::prefs::UseUpmLocalAndSeparateStoresState;

namespace password_manager {

bool UsesSplitStoresAndUPMForLocal(PrefService* pref_service) {
  switch (
      static_cast<UseUpmLocalAndSeparateStoresState>(pref_service->GetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores))) {
    case UseUpmLocalAndSeparateStoresState::kOff:
    case UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
      return false;
    case UseUpmLocalAndSeparateStoresState::kOn:
      return true;
  }
  NOTREACHED_NORETURN();
}

}  // namespace password_manager
