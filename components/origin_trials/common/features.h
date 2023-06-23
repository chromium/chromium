// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_COMMON_FEATURES_H_
#define COMPONENTS_ORIGIN_TRIALS_COMMON_FEATURES_H_

#include "base/feature_list.h"

namespace origin_trials::features {

// Helper to check if |::features::kPersistentOriginTrials| from
// |//content/public/common/content_features.h| is enabled.
bool IsPersistentOriginTrialsEnabled();

}  // namespace origin_trials::features

#endif  // COMPONENTS_ORIGIN_TRIALS_COMMON_FEATURES_H_
