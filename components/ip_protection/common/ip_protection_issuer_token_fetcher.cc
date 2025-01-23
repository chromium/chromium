// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_issuer_token_fetcher.h"

namespace ip_protection {

TryGetIssuerTokensOutcome::TryGetIssuerTokensOutcome() = default;
TryGetIssuerTokensOutcome::~TryGetIssuerTokensOutcome() = default;
TryGetIssuerTokensOutcome::TryGetIssuerTokensOutcome(
    const TryGetIssuerTokensOutcome& other) = default;
TryGetIssuerTokensOutcome::TryGetIssuerTokensOutcome(
    TryGetIssuerTokensOutcome&& other) = default;
TryGetIssuerTokensOutcome& TryGetIssuerTokensOutcome::operator=(
    const TryGetIssuerTokensOutcome&) = default;
TryGetIssuerTokensOutcome& TryGetIssuerTokensOutcome::operator=(
    TryGetIssuerTokensOutcome&&) = default;

}  // namespace ip_protection
