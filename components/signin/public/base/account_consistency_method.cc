// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/account_consistency_method.h"

#include "base/logging.h"

namespace signin {

namespace {
bool AccountConsistencyMethodGreaterOrEqual(AccountConsistencyMethod a,
                                            AccountConsistencyMethod b) {
  return static_cast<int>(a) >= static_cast<int>(b);
}
}  // namespace

#if defined(OS_ANDROID)
const base::Feature kMiceFeature{"MobileIdentityConsistency",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
#endif

bool DiceMethodGreaterOrEqual(AccountConsistencyMethod a,
                              AccountConsistencyMethod b) {
  DCHECK_NE(AccountConsistencyMethod::kMirror, a);
  DCHECK_NE(AccountConsistencyMethod::kMirror, b);
  return AccountConsistencyMethodGreaterOrEqual(a, b);
}

}  // namespace signin
