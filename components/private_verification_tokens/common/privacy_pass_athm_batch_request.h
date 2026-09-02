// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVACY_PASS_ATHM_BATCH_REQUEST_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVACY_PASS_ATHM_BATCH_REQUEST_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"

namespace private_verification_tokens {

// Represents errors that can occur during batch request generation or response
// finalization.
enum class PrivacyPassAthmBatchRequestError {
  kInvalidBatchSize,
  kInvalidBucketCount,
  kClientRequestGenerationFailed,
  kAlreadyFinalized,
  kInvalidResponseBodyLength,
  kClientFinalizeFailed,
};

// Encapsulates a batched Privacy Pass request using Anonymous Tokens with
// Hidden Metadata (ATHM).
//
// A batch request is composed of multiple Privacy Pass ATHM token requests
// (type 0xC07E) concatenated into a single request body. This class generates
// and holds the client-side secret contexts and blinded requests produced by
// the underlying Rust ATHM FFI, marshals the wire encodings, and upon receiving
// the server's batch response, unblinds and finalizes the tokens into Privacy
// Pass format.
//
// This is a single-use object: once Finalize() is invoked, the internal secret
// contexts are consumed and discarded.
class PrivacyPassAthmBatchRequest {
 public:
  // Constructs and initializes a batch request for the issuer described by
  // `issuer_config` and `num_buckets` (the metadata bucket count).
  // Protocol parameters are derived internally from
  // `issuer_config.deployment_id` and `num_buckets`.
  //
  // Returns PrivacyPassAthmBatchRequest on success, or a
  // PrivacyPassAthmBatchRequestError describing why creation failed.
  static base::expected<PrivacyPassAthmBatchRequest,
                        PrivacyPassAthmBatchRequestError>
  Create(const IssuerConfig& issuer_config, uint8_t num_buckets);

  PrivacyPassAthmBatchRequest(PrivacyPassAthmBatchRequest&&);
  PrivacyPassAthmBatchRequest& operator=(PrivacyPassAthmBatchRequest&&);
  ~PrivacyPassAthmBatchRequest();

  PrivacyPassAthmBatchRequest(const PrivacyPassAthmBatchRequest&) = delete;
  PrivacyPassAthmBatchRequest& operator=(const PrivacyPassAthmBatchRequest&) =
      delete;

  // The serialized HTTP request body containing the concatenated marshaled
  // AthmTokenRequest structures (each 36 bytes) for all tokens in the batch
  // followed by the 4-byte version number in big-endian order.
  const std::vector<uint8_t>& request_body() const { return request_body_; }

  // Unblinds and finalizes the batch response returned by the issuance server.
  // `response_body` contains the concatenated serialized token responses.
  //
  // On success, returns the list of marshaled AthmToken byte vectors (132 bytes
  // each: 2-byte token type, 32-byte SHA256 issuer key ID, 98-byte unblinded
  // token) and invalidates the internal secret contexts so it cannot be called
  // again.
  // Returns PrivacyPassAthmBatchRequestError on failure.
  base::expected<std::vector<std::vector<uint8_t>>,
                 PrivacyPassAthmBatchRequestError>
  Finalize(base::span<const uint8_t> response_body);

 private:
  explicit PrivacyPassAthmBatchRequest(PrivacyPassBatchClient batch_client);

  std::optional<PrivacyPassBatchClient> batch_client_;
  std::vector<uint8_t> request_body_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVACY_PASS_ATHM_BATCH_REQUEST_H_
