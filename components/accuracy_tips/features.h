// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_FEATURES_H_
#define COMPONENTS_ACCURACY_TIPS_FEATURES_H_

#include "base/metrics/field_trial_params.h"

namespace accuracy_tips {
namespace features {

// Additional parameters for safe_browsing::kAccuracyTipsFeature.

// URL that triggers an AccuracyTip for testing purposes.
extern const base::FeatureParam<std::string> kSampleUrl;

// Disables the UI but still queries SB and records metrics. Used for
// dark-launch and to create a control group.
extern const base::FeatureParam<bool> kDisableUi;

// URL that the "Learn more" button links to.
extern const base::FeatureParam<std::string> kLearnMoreUrl;

// Amount of time that has to pass between two accuracy prompts.
extern const base::FeatureParam<base::TimeDelta> kTimeBetweenPrompts;

}  // namespace features
}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_FEATURES_H_