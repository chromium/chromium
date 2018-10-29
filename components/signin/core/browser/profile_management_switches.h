// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_MANAGEMENT_SWITCHES_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_MANAGEMENT_SWITCHES_H_

namespace signin {

// TODO(https://crbug.com/777774): Cleanup this enum and remove related
// functions once Dice is fully rolled out, and/or Mirror code is removed on
// desktop.
enum class AccountConsistencyMethod : int {
  // No account consistency.
  kDisabled,

  // Account management UI in the avatar bubble.
  kMirror,

  // No account consistency, but Dice fixes authentication errors.
  kDiceFixAuthErrors,

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

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_MANAGEMENT_SWITCHES_H_
