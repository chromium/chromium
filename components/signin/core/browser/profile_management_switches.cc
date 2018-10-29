// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_management_switches.h"

#include "base/logging.h"

namespace signin {

namespace {
bool AccountConsistencyMethodGreaterOrEqual(AccountConsistencyMethod a,
                                            AccountConsistencyMethod b) {
  return static_cast<int>(a) >= static_cast<int>(b);
}
}  // namespace

bool DiceMethodGreaterOrEqual(AccountConsistencyMethod a,
                              AccountConsistencyMethod b) {
  DCHECK_NE(AccountConsistencyMethod::kMirror, a);
  DCHECK_NE(AccountConsistencyMethod::kMirror, b);
  return AccountConsistencyMethodGreaterOrEqual(a, b);
}

}  // namespace signin
