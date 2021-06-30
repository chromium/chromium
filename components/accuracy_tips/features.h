// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_FEATURES_H_
#define COMPONENTS_ACCURACY_TIPS_FEATURES_H_

#include "base/metrics/field_trial_params.h"

namespace accuracy_tips {

// Additional parameters for safe_browsing::kAccuracyTipsFeature.
extern const base::FeatureParam<std::string> kSampleUrl;

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_FEATURES_H_