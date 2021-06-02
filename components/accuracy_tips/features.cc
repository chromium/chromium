// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace accuracy_tips {

const base::Feature kAccuracyTipsFeature{"AccuracyTips",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kSampleUrl{&kAccuracyTipsFeature,
                                                 "sampleUrl", ""};

}  // namespace accuracy_tips