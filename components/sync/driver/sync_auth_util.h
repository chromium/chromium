// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_

#include "components/signin/public/identity_manager/account_info.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {

struct SyncAccountInfo {
  SyncAccountInfo();
  SyncAccountInfo(const CoreAccountInfo& account_info, bool is_sync_consented);

  CoreAccountInfo account_info;
  bool is_sync_consented = false;
};

// Determines which account should be used for Sync and returns the
// corresponding SyncAccountInfo. This is exposed so that autofill metrics
// code can use it.
SyncAccountInfo DetermineAccountToUse(
    signin::IdentityManager* identity_manager);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_
