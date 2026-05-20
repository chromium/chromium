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
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"

namespace private_verification_tokens::internal {

bool IsRegistrableDomain(std::string_view domain);

std::optional<int> GetValidVersion(const base::DictValue& dict);

std::optional<std::vector<uint8_t>> GetDecodedPublicKey(
    const base::DictValue& dict);

std::optional<uint32_t> GetValidKeyId(const base::DictValue& dict);

std::optional<int> GetValidBatchSize(
    const base::DictValue& dict,
    const PrivateVerificationTokensParameters& params);

std::optional<int64_t> GetValidExpiration(const base::DictValue& dict);

}  // namespace private_verification_tokens::internal

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_INTERNAL_H_
