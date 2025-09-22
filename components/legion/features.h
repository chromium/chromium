// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_FEATURES_H_
#define COMPONENTS_LEGION_FEATURES_H_

#include "base/features.h"
#include "base/metrics/field_trial_params.h"

namespace legion {

// The feature for Legion.
BASE_DECLARE_FEATURE(kLegion);

// The API key for Legion.
extern const base::FeatureParam<std::string> kLegionApiKey;

// Endpoint for Legion
extern const base::FeatureParam<std::string> kLegionUrl;

}  // namespace legion

#endif  // COMPONENTS_LEGION_FEATURES_H_
