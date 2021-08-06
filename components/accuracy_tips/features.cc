// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/features.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/features.h"

namespace accuracy_tips {
namespace features {

const base::FeatureParam<std::string> kSampleUrl{
    &safe_browsing::kAccuracyTipsFeature, "SampleUrl", ""};

const base::FeatureParam<bool> kDisableUi{&safe_browsing::kAccuracyTipsFeature,
                                          "DisableUI", false};

const base::FeatureParam<std::string> kLearnMoreUrl{
    &safe_browsing::kAccuracyTipsFeature, "LearnMoreUrl", ""};

const base::FeatureParam<base::TimeDelta> kTimeBetweenPrompts{
    &safe_browsing::kAccuracyTipsFeature, "TimeBetweenPrompts",
    base::TimeDelta::FromDays(7)};

extern const base::FeatureParam<int> kNumIgnorePrompts{
    &safe_browsing::kAccuracyTipsFeature, "NumIgnorePrompts", 2};

}  // namespace features
}  // namespace accuracy_tips
