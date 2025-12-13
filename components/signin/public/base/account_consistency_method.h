// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_ACCOUNT_CONSISTENCY_METHOD_H_

namespace signin {

// Defines the account consistency methods used for managing Google accounts
// within Chrome. These methods determine how account management UI is presented
// and how account consistency is enforced.
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
