// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_FEATURES_H_
#define COMPONENTS_DRIVE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace drive::features {

BASE_DECLARE_FEATURE(kEnablePollingInterval);

extern const base::FeatureParam<int> kPollingIntervalInSecs;

}  // namespace drive::features

#endif  // COMPONENTS_DRIVE_FEATURES_H_
