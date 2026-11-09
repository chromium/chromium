// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_FEATURES_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_FEATURES_H_

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace private_verification_tokens {

// Whether to prefetch Private Verification Tokens through the PVT proxy
// service when the local token cache for a registered issuer is empty.
extern const base::FeatureParam<bool> kPrivateVerificationTokensUseProxy;

// The URL of the PVT proxy endpoint used to prefetch Private Verification
// Tokens.
extern const base::FeatureParam<std::string> kPrivateVerificationTokensProxyUrl;

// The interval at which the service checks registered issuers and prefetches
// proxied Private Verification Tokens if their token cache is empty.
extern const base::FeatureParam<base::TimeDelta>
    kPrivateVerificationTokensProxyRefreshInterval;

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_FEATURES_H_
