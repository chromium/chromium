// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace signin {

#if defined(OS_ANDROID)
// Mice is similar to Mirror but also works when the user is not opted into
// Sync.
extern const base::Feature kMiceFeature;
#endif

// TODO(https://crbug.com/777774): Cleanup this enum and remove related
// functions once Dice is fully rolled out, and/or Mirror code is removed on
// desktop.
enum class AccountConsistencyMethod : int {
  // No account consistency.
  kDisabled,

  // Account management UI in the avatar bubble.
  kMirror,

  // Account management UI on Gaia webpages is enabled once the accounts become
  // consistent.
  kDiceMigration,

  // Account management UI on Gaia webpages is enabled. If accounts are not
  // consistent when this is enabled, the account reconcilor enforces the
  // consistency.
  kDice
};

// Returns true if the |a| comes after |b| in the AccountConsistencyMethod enum.
// Should not be used for Mirror.
bool DiceMethodGreaterOrEqual(AccountConsistencyMethod a,
                              AccountConsistencyMethod b);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_
