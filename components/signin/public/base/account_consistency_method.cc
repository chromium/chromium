// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/account_consistency_method.h"


namespace signin {

// Do not merge the two feature flags.
// Experiments for MICE will be run independently per platform (Android, iOS).
#if defined(OS_ANDROID)
// Feature flag for FRE related changes as part of MICE.
const base::Feature kMobileIdentityConsistencyFRE{
    "MobileIdentityConsistencyFRE", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kMobileIdentityConsistencyPromos{
    "MobileIdentityConsistencyPromos", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

}  // namespace signin
