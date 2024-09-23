// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/features.h"

namespace drive::features {

BASE_FEATURE(kEnablePollingInterval,
             "EnablePollingInterval",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kPollingIntervalInSecs{
    &kEnablePollingInterval, "PollingIntervalInSecs", /*default_value=*/60};

}  // namespace drive::features
