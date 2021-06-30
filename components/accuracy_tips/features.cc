// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/features.h"

namespace accuracy_tips {

const base::FeatureParam<std::string> kSampleUrl{
    &safe_browsing::kAccuracyTipsFeature, "sampleUrl", ""};

}  // namespace accuracy_tips