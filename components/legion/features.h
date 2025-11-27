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

// Sets the name of the Legion auth token server.
extern const base::FeatureParam<std::string> kLegionTokenServerUrl;

// Sets the path component of the Legion auth token server URL used for
// getting initial token signing data.
extern const base::FeatureParam<std::string>
    kLegionTokenServerGetInitialDataPath;

// Sets the path component of the Legion auth token server URL used for
// getting blind-signed tokens.
extern const base::FeatureParam<std::string> kLegionTokenServerGetTokensPath;

}  // namespace legion

#endif  // COMPONENTS_LEGION_FEATURES_H_
