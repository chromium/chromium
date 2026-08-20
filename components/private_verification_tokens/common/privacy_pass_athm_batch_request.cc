// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/privacy_pass_athm_batch_request.h"

#include <array>
#include <string>
#include <utility>

#include "base/containers/to_vector.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"
#include "crypto/hash.h"
#include "third_party/anonymous_tokens/src/anonymous_tokens/cpp/privacy_pass/athm_token_encodings_utils.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"
#include "third_party/crubit/support/rs_std/vec.h"

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

  // Derive client protocol parameters using the deployment ID and bucket count.
  auto params = AthmParameters::try_new(
      num_buckets, rs_std::SliceRef<const uint8_t>(
                       base::as_byte_span(issuer_config.deployment_id)));
  if (!params.has_value()) {
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kParameterGenerationFailed);
  }

  const size_t count = static_cast<size_t>(issuer_config.batch_size);
  base::span<const uint8_t> public_key = issuer_config.public_key.public_key();
  base::span<const uint8_t> public_key_proof =
      issuer_config.public_key.public_key_proof();

  std::vector<AthmClientRequest> client_requests;
  client_requests.reserve(count);
  std::vector<uint8_t> batch_request_body;

  for (size_t i = 0; i < count; ++i) {
    auto bridge_result = AthmClientRequest::try_new(
        rs_std::SliceRef<const uint8_t>(public_key),
        rs_std::SliceRef<const uint8_t>(public_key_proof), *params);
    if (!bridge_result.has_value()) {
      return base::unexpected(
          PrivacyPassAthmBatchRequestError::kClientRequestGenerationFailed);
    }

    rs_std::Vec<uint8_t> req_bytes = bridge_result->request();
    anonymous_tokens::AthmTokenRequest token_req;
    token_req.token_type = PrivateVerificationTokensParameters::kAthmTokenType;
    token_req.truncated_issuer_key_id =
        issuer_config.public_key.truncated_key_id();
    token_req.encoded_request = std::string(req_bytes.begin(), req_bytes.end());

    std::string marshaled_req;
    absl::Status status =
        anonymous_tokens::MarshalAthmTokenRequest(token_req, &marshaled_req);
    if (!status.ok()) {
      return base::unexpected(
          PrivacyPassAthmBatchRequestError::kTokenRequestEncodingFailed);
    }

    batch_request_body.insert(batch_request_body.end(), marshaled_req.begin(),
                              marshaled_req.end());
    client_requests.push_back(std::move(*bridge_result));
  }

  return PrivacyPassAthmBatchRequest(
      issuer_config.public_key, std::move(*params), std::move(client_requests),
      std::move(batch_request_body));
}

PrivacyPassAthmBatchRequest::PrivacyPassAthmBatchRequest(
    PrivateVerificationTokensPublicKey pvt_public_key,
    AthmParameters params,
    std::vector<AthmClientRequest> client_requests,
    std::vector<uint8_t> request_body)
    : pvt_public_key_(std::move(pvt_public_key)),
      params_(std::move(params)),
      client_requests_(std::move(client_requests)),
      request_body_(std::move(request_body)) {}

PrivacyPassAthmBatchRequest::PrivacyPassAthmBatchRequest(
    PrivacyPassAthmBatchRequest&&) = default;
PrivacyPassAthmBatchRequest& PrivacyPassAthmBatchRequest::operator=(
    PrivacyPassAthmBatchRequest&&) = default;
PrivacyPassAthmBatchRequest::~PrivacyPassAthmBatchRequest() = default;

base::expected<std::vector<std::vector<uint8_t>>,
               PrivacyPassAthmBatchRequestError>
PrivacyPassAthmBatchRequest::Finalize(base::span<const uint8_t> response_body) {
  if (client_requests_.empty()) {
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kAlreadyFinalized);
  }
  const size_t token_count = client_requests_.size();
  const size_t single_response_size = params_.token_response_size();

  const size_t expected_response_size = token_count * single_response_size;
  if (response_body.size() != expected_response_size) {
    return base::unexpected(
        PrivacyPassAthmBatchRequestError::kInvalidResponseBodyLength);
  }

  const std::array<uint8_t, crypto::hash::kSha256Size> issuer_key_id =
      pvt_public_key_.key_id();
  const std::string issuer_key_id_str(issuer_key_id.begin(),
                                      issuer_key_id.end());

  std::vector<std::vector<uint8_t>> finalized_tokens;
  finalized_tokens.reserve(client_requests_.size());

  // Consume client_requests_ so this instance cannot be finalized again.
  std::vector<AthmClientRequest> requests = std::exchange(client_requests_, {});

  for (size_t i = 0; i < token_count; ++i) {
    base::span<const uint8_t> item_response =
        response_body.subspan(i * single_response_size, single_response_size);
    auto finalize_result = requests[i].finalize(
        rs_std::SliceRef<const uint8_t>(pvt_public_key_.public_key()),
        rs_std::SliceRef<const uint8_t>(item_response), params_);
    if (!finalize_result.has_value()) {
      return base::unexpected(
          PrivacyPassAthmBatchRequestError::kClientFinalizeFailed);
    }

    anonymous_tokens::AthmToken token;
    token.token_type = PrivateVerificationTokensParameters::kAthmTokenType;
    token.issuer_key_id = issuer_key_id_str;
    token.token = std::string(finalize_result->begin(), finalize_result->end());

    std::string marshaled_token;
    absl::Status status =
        anonymous_tokens::MarshalAthmToken(token, &marshaled_token);
    if (!status.ok()) {
      return base::unexpected(
          PrivacyPassAthmBatchRequestError::kTokenEncodingFailed);
    }

    finalized_tokens.push_back(
        base::ToVector(base::as_byte_span(marshaled_token)));
  }

  return finalized_tokens;
}

}  // namespace private_verification_tokens
