// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/features.h"

namespace private_ai {

BASE_FEATURE(kPrivateAi, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateAiServerAttestation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateAiUseTokenAttestation, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPrivateAiApiKey{
    &kPrivateAi, /*name=*/"api-key", /*default_value=*/""};

const base::FeatureParam<std::string> kPrivateAiUrl{
    &kPrivateAi, /*name=*/"url",
    /*default_value=*/
    "privatearatea-pa.googleapis.com/ws/"
    "mdi.privatearatea.PrivateArateaService.StartNoiseSession"};

const base::FeatureParam<std::string> kPrivateAiProxyServerUrl{
    &kPrivateAi, /*name=*/"proxy-url", /*default_value=*/""};

const base::FeatureParam<std::string> kPrivateAiTokenServerUrl{
    &kPrivateAi, /*name=*/"token-server-url",
    /*default_value=*/"https://phosphor-pa.googleapis.com"};

const base::FeatureParam<std::string> kPrivateAiTokenServerGetInitialDataPath{
    &kPrivateAi,
    /*name=*/"token-server-get-initial-data-path",
    /*default_value=*/"/v1/privatearateaipp/getInitialData"};

const base::FeatureParam<std::string> kPrivateAiTokenServerGetTokensPath{
    &kPrivateAi, /*name=*/"token-server-get-tokens-path",
    /*default_value=*/"/v1/privatearateaipp/auth-chrome"};

const base::FeatureParam<int> kPrivateAiAuthTokenCacheBatchSize{
    &kPrivateAi, /*name=*/"auth-token-cache-batch-size",
    /*default_value=*/5};

const base::FeatureParam<int> kPrivateAiAuthTokenCacheLowWaterMark{
    &kPrivateAi, /*name=*/"auth-token-cache-low-water-mark",
    /*default_value=*/1};

const base::FeatureParam<base::TimeDelta>
    kPrivateAiTryGetAuthTokensNotEligibleBackoff{
        &kPrivateAi,
        /*name=*/"try-get-auth-tokens-not-eligible-backoff",
        /*default_value=*/base::Hours(1)};

const base::FeatureParam<base::TimeDelta>
    kPrivateAiTryGetAuthTokensTransientBackoff{
        &kPrivateAi,
        /*name=*/"try-get-auth-tokens-transient-backoff",
        /*default_value=*/base::Seconds(5)};

const base::FeatureParam<base::TimeDelta> kPrivateAiTryGetAuthTokensBugBackoff{
    &kPrivateAi, /*name=*/"try-get-auth-tokens-bug-backoff",
    /*default_value=*/base::Minutes(10)};

const base::FeatureParam<double> kPrivateAiBackoffJitter{
    &kPrivateAi, /*name=*/"backoff-jitter", /*default_value=*/0.25};

}  // namespace private_ai
