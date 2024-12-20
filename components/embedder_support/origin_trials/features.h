// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_FEATURES_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_FEATURES_H_

#include "base/feature_list.h"

namespace embedder_support {

// Sample field trial feature for testing alternative usage restriction in
// origin trial third party tokens.
//  - While not used in production code, the feature must be kept for tests in
//    components/embedder_support/origin_trials/origin_trial_policy_impl_unittest.cc.
BASE_DECLARE_FEATURE(kOriginTrialsSampleAPIThirdPartyAlternativeUsage);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_FEATURES_H_
