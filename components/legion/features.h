// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_FEATURES_H_
#define COMPONENTS_LEGION_FEATURES_H_

#include "base/features.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace private_ai {

// The feature for Legion.
BASE_DECLARE_FEATURE(kLegion);

// Feature flag to enable server attestation.
BASE_DECLARE_FEATURE(kLegionSeverAttestation);

// The API key for Legion.
extern const base::FeatureParam<std::string> kLegionApiKey;

// Endpoint for Legion
extern const base::FeatureParam<std::string> kLegionUrl;

// Endpoint for the Legion Proxy Server.
extern const base::FeatureParam<std::string> kLegionProxyServerUrl;

// Sets the name of the Legion auth token server.
extern const base::FeatureParam<std::string> kLegionTokenServerUrl;

// Sets the path component of the Legion auth token server URL used for
// getting initial token signing data.
extern const base::FeatureParam<std::string>
    kLegionTokenServerGetInitialDataPath;

// Sets the path component of the Legion auth token server URL used for
// getting blind-signed tokens.
extern const base::FeatureParam<std::string> kLegionTokenServerGetTokensPath;

// The number of auth tokens to request in each batch.
extern const base::FeatureParam<int> kLegionAuthTokenCacheBatchSize;

// The number of available auth tokens below which a new batch of tokens will be
// requested.
extern const base::FeatureParam<int> kLegionAuthTokenCacheLowWaterMark;

// The backoff duration for fetching auth tokens if the user is not eligible.
extern const base::FeatureParam<base::TimeDelta>
    kLegionTryGetAuthTokensNotEligibleBackoff;

// The initial backoff for fetching auth tokens after a transient error.
extern const base::FeatureParam<base::TimeDelta>
    kLegionTryGetAuthTokensTransientBackoff;

// The initial backoff for fetching auth tokens after an error that suggests a
// bug.
extern const base::FeatureParam<base::TimeDelta>
    kLegionTryGetAuthTokensBugBackoff;

// The jitter factor to apply to backoff durations.
extern const base::FeatureParam<double> kLegionBackoffJitter;

}  // namespace private_ai

#endif  // COMPONENTS_LEGION_FEATURES_H_
