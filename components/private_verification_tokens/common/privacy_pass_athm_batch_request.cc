// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/privacy_pass_athm_batch_request.h"

#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"

namespace private_verification_tokens {

// static
base::expected<PrivacyPassAthmBatchRequest, PrivacyPassAthmBatchRequestError>
PrivacyPassAthmBatchRequest::Create(const IssuerConfig& issuer_config,
                                    uint8_t num_buckets) {
  if (issuer_config.batch_size <= 0) {
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kInvalidBatchSize);
  }
  if (num_buckets == 0) {
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kInvalidBucketCount);
  }

  auto client_result = PrivacyPassBatchClient::try_new(
      rs_std::SliceRef<const uint8_t>(issuer_config.public_key.public_key()),
      rs_std::SliceRef<const uint8_t>(
          issuer_config.public_key.public_key_proof()),
      num_buckets,
      rs_std::SliceRef<const uint8_t>(
          base::as_byte_span(issuer_config.deployment_id)),
      issuer_config.public_key.version(),
      static_cast<size_t>(issuer_config.batch_size));
  if (!client_result.has_value()) {
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kClientRequestGenerationFailed);
  }

  return PrivacyPassAthmBatchRequest(std::move(*client_result));
}

PrivacyPassAthmBatchRequest::PrivacyPassAthmBatchRequest(
    PrivacyPassBatchClient batch_client)
    : batch_client_(std::move(batch_client)),
      request_body_(base::ToVector(batch_client_->request_body().to_span())) {}

PrivacyPassAthmBatchRequest::PrivacyPassAthmBatchRequest(
    PrivacyPassAthmBatchRequest&&) = default;
PrivacyPassAthmBatchRequest& PrivacyPassAthmBatchRequest::operator=(
    PrivacyPassAthmBatchRequest&&) = default;
PrivacyPassAthmBatchRequest::~PrivacyPassAthmBatchRequest() = default;

base::expected<std::vector<std::vector<uint8_t>>,
               PrivacyPassAthmBatchRequestError>
PrivacyPassAthmBatchRequest::Finalize(base::span<const uint8_t> response_body) {
  if (!batch_client_.has_value()) {
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kAlreadyFinalized);
  }

  auto finalize_result =
      std::exchange(batch_client_, std::nullopt)
          ->finalize(rs_std::SliceRef<const uint8_t>(response_body));
  if (!finalize_result.has_value()) {
    if (finalize_result.error() == AthmStatus::MakeInvalidInput()) {
      return base::unexpected(
          PrivacyPassAthmBatchRequestError::kInvalidResponseBodyLength);
    }
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kClientFinalizeFailed);
  }

  std::vector<std::vector<uint8_t>> finalized_tokens;
  finalized_tokens.reserve(finalize_result->size());
  for (const auto& token : *finalize_result) {
    finalized_tokens.push_back(base::ToVector(token));
  }
  return finalized_tokens;
}

}  // namespace private_verification_tokens
