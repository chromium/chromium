// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"

namespace ip_protection {

TryGetProbabilisticRevealTokensOutcome::
    TryGetProbabilisticRevealTokensOutcome() = default;
TryGetProbabilisticRevealTokensOutcome::
    ~TryGetProbabilisticRevealTokensOutcome() = default;
TryGetProbabilisticRevealTokensOutcome::TryGetProbabilisticRevealTokensOutcome(
    const TryGetProbabilisticRevealTokensOutcome& other) = default;
TryGetProbabilisticRevealTokensOutcome::TryGetProbabilisticRevealTokensOutcome(
    TryGetProbabilisticRevealTokensOutcome&& other) = default;
TryGetProbabilisticRevealTokensOutcome&
TryGetProbabilisticRevealTokensOutcome::operator=(
    const TryGetProbabilisticRevealTokensOutcome&) = default;
TryGetProbabilisticRevealTokensOutcome&
TryGetProbabilisticRevealTokensOutcome::operator=(
    TryGetProbabilisticRevealTokensOutcome&&) = default;

}  // namespace ip_protection
