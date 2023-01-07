// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_

namespace signin {

enum class AccountConsistencyMethod : int {
  // No account consistency.
  kDisabled,

  // Account management UI in the avatar bubble.
  kMirror,

  // Account management UI on Gaia webpages is enabled. If accounts are not
  // consistent when this is enabled, the account reconcilor enforces the
  // consistency.
  kDice
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_
