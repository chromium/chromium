// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"

#include <cstdint>
#include <optional>

namespace private_verification_tokens {

std::optional<PrivateVerificationTokensParameters> GetParametersForVersion(
    uint32_t version) {
  if (version == 1) {
    return PrivateVerificationTokensParameters{
        .min_batch_size = 2,
        .max_batch_size = 20,
    };
  }
  return std::nullopt;
}

}  // namespace private_verification_tokens
