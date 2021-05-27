// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/account_consistency_method.h"


namespace signin {

// Do not merge the two feature flags.
// Experiments for MICE will be run independently per platform (Android, iOS).
#if defined(OS_ANDROID)
const base::Feature kMobileIdentityConsistency{
    "MobileIdentityConsistency", base::FEATURE_ENABLED_BY_DEFAULT};
// This feature flag is used to run experiments of different variations
// of MICE on Android.
const base::Feature kMobileIdentityConsistencyVar{
    "MobileIdentityConsistencyVar", base::FEATURE_DISABLED_BY_DEFAULT};
// Feature flag for FRE related changes as part of MICE.
const base::Feature kMobileIdentityConsistencyFRE{
    "MobileIdentityConsistencyFRE", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kMobileIdentityConsistencyPromos{
    "MobileIdentityConsistencyPromos", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_IOS)
const base::Feature kMobileIdentityConsistency{
    "MobileIdentityConsistency", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMICEWebSignIn{"MICEWebSignInEnabled",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_IOS)

#if defined(OS_ANDROID) || defined(OS_IOS)
bool IsMobileIdentityConsistencyEnabled() {
  return base::FeatureList::IsEnabled(kMobileIdentityConsistency);
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_IOS)
bool IsMICEWebSignInEnabled() {
  return IsMobileIdentityConsistencyEnabled() &&
         base::FeatureList::IsEnabled(kMICEWebSignIn);
}
#endif  // defined(OS_IOS)

}  // namespace signin
