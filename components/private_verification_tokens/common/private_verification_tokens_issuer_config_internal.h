// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_INTERNAL_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_INTERNAL_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"

namespace private_verification_tokens::internal {

std::optional<int> GetValidVersion(const base::DictValue& dict);

std::optional<std::vector<uint8_t>> GetDecodedPublicKey(
    const base::DictValue& dict);

std::optional<int> GetValidBatchSize(
    const base::DictValue& dict,
    const PrivateVerificationTokensParameters& params);

std::optional<int64_t> GetValidExpiration(const base::DictValue& dict);

// Parses and returns the vector of redeemer origins from the 'redeemers'
// field in the config dictionary.
// GetValidRedeemers returns std::nullopt in the following cases:
// - The 'redeemers' field is missing from `dict`.
// - The redeemer list size is larger than params.max_number_of_redeemers.
// - An item in the redeemer list is not a string.
// - The resulting origin for a redeemer does not have an HTTPS scheme.
//   (This condition also covers opaque and invalid origins.)
// - A redeemer has an eTLD+1 different from the issuer's eTLD+1.
std::optional<std::vector<url::Origin>> GetValidRedeemers(
    const base::DictValue& dict,
    std::string_view issuer_etld_plus_one,
    const PrivateVerificationTokensParameters& params);

std::optional<IssuerConfig> ParseEntry(const base::DictValue& entry);

}  // namespace private_verification_tokens::internal

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_INTERNAL_H_
