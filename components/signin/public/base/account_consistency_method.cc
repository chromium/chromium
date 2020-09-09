// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/account_consistency_method.h"


namespace signin {

// Do not merge the two feature flags.
// Experiments for MICE will be run independently per platform (Android, iOS).
#if defined(OS_ANDROID)
const base::Feature kMobileIdentityConsistency{
    "MobileIdentityConsistency", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_IOS)
const base::Feature kMobileIdentityConsistency{
    "MobileIdentityConsistency", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace signin
