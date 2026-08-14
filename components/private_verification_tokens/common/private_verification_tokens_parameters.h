// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PARAMETERS_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PARAMETERS_H_

#include <cstddef>
#include <cstdint>
#include <optional>

namespace private_verification_tokens {

// Holds all PVT parameters.
struct PrivateVerificationTokensParameters {
  static constexpr uint16_t kAthmTokenType = 0xC07E;

  // Minimum acceptable batch size.
  int min_batch_size = 0;
  // Maximum acceptable batch size.
  int max_batch_size = 0;
  // Limit for maximum number of redeemers.
  int max_number_of_redeemers = 0;
  // Number of metadata buckets.
  uint8_t num_buckets = 0;
  // Size of a single serialized token request in bytes.
  size_t single_request_size = 0;
  // Size of the blinded token request in bytes.
  size_t blinded_request_size = 0;
};

// Returns the parameters for a given version, or nullopt if the version is not
// supported.
std::optional<PrivateVerificationTokensParameters> GetParametersForVersion(
    uint32_t version);

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PARAMETERS_H_
