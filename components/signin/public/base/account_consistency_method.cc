// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/account_consistency_method.h"

#include "build/build_config.h"

namespace signin {

#if BUILDFLAG(IS_ANDROID)
const base::Feature kMobileIdentityConsistencyPromos{
    "MobileIdentityConsistencyPromos", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

}  // namespace signin
