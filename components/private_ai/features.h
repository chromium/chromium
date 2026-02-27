// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_FEATURES_H_
#define COMPONENTS_PRIVATE_AI_FEATURES_H_

#include "base/features.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace private_ai {

// The feature for Private AI.
BASE_DECLARE_FEATURE(kPrivateAi);

// Feature flag to enable server attestation.
BASE_DECLARE_FEATURE(kPrivateAiServerAttestation);

// Feature flag to enable token attestation.
BASE_DECLARE_FEATURE(kPrivateAiUseTokenAttestation);

// The API key for Private AI.
extern const base::FeatureParam<std::string> kPrivateAiApiKey;

// Endpoint for Private AI
extern const base::FeatureParam<std::string> kPrivateAiUrl;

// Endpoint for the Private AI Proxy Server.
extern const base::FeatureParam<std::string> kPrivateAiProxyServerUrl;

// Sets the name of the Private AI auth token server.
extern const base::FeatureParam<std::string> kPrivateAiTokenServerUrl;

// Sets the path component of the Private AI auth token server URL used for
// getting initial token signing data.
extern const base::FeatureParam<std::string>
    kPrivateAiTokenServerGetInitialDataPath;

// Sets the path component of the Private AI auth token server URL used for
// getting blind-signed tokens.
extern const base::FeatureParam<std::string> kPrivateAiTokenServerGetTokensPath;

// The number of auth tokens to request in each batch.
extern const base::FeatureParam<int> kPrivateAiAuthTokenCacheBatchSize;

// The number of available auth tokens below which a new batch of tokens will be
// requested.
extern const base::FeatureParam<int> kPrivateAiAuthTokenCacheLowWaterMark;

// The backoff duration for fetching auth tokens if the user is not eligible.
extern const base::FeatureParam<base::TimeDelta>
    kPrivateAiTryGetAuthTokensNotEligibleBackoff;

// The initial backoff for fetching auth tokens after a transient error.
extern const base::FeatureParam<base::TimeDelta>
    kPrivateAiTryGetAuthTokensTransientBackoff;

// The initial backoff for fetching auth tokens after an error that suggests a
// bug.
extern const base::FeatureParam<base::TimeDelta>
    kPrivateAiTryGetAuthTokensBugBackoff;

// The jitter factor to apply to backoff durations.
extern const base::FeatureParam<double> kPrivateAiBackoffJitter;

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_FEATURES_H_
