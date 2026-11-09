// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_features.h"

#include "net/base/features.h"

namespace private_verification_tokens {

const base::FeatureParam<bool> kPrivateVerificationTokensUseProxy{
    &net::features::kEnablePrivateVerificationTokens, "use_proxy", false};

const base::FeatureParam<std::string> kPrivateVerificationTokensProxyUrl{
    &net::features::kEnablePrivateVerificationTokens, "proxy_url",
    "https://pvtproxy.googleapis.com/v1:proxyRequest"};

const base::FeatureParam<base::TimeDelta>
    kPrivateVerificationTokensProxyRefreshInterval{
        &net::features::kEnablePrivateVerificationTokens,
        "proxy_refresh_interval", base::Hours(5)};

}  // namespace private_verification_tokens
