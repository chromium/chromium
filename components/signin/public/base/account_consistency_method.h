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

#if defined(OS_ANDROID) || defined(OS_IOS)
// Mice is similar to Mirror but also works when the user is not opted into
// Sync.
extern const base::Feature kMobileIdentityConsistency;
// Returns true if the flag |kMobileIdentityConsistency| is enabled for the
// platform.
bool IsMobileIdentityConsistencyEnabled();

// Feature flag for FRE related changes as part of MICE.
extern const base::Feature kMobileIdentityConsistencyFRE;

// Feature flag for promo-related changes of `kMobileIdentityConsistency`.
extern const base::Feature kMobileIdentityConsistencyPromos;
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_IOS)
// Feature flag for promo-related changes of `kMobileIdentityConsistency`.
extern const base::Feature kMICEWebSignIn;

// Returns true if the flags |kMICEWebSignInEnabled| and
// |kMobileIdentityConsistency| are enabled for the platform.
bool IsMICEWebSignInEnabled();
#endif  // defined(OS_IOS)

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
