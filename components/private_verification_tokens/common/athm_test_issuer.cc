// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/athm_test_issuer.h"

#include <utility>

#include "base/containers/span_rust.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/private_verification_tokens/common/athm_ffi.rs.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "crypto/hash.h"
#include "third_party/anonymous_tokens/src/anonymous_tokens/cpp/privacy_pass/athm_token_encodings_utils.h"

namespace private_verification_tokens {

// static
std::optional<AthmTestIssuer> AthmTestIssuer::Create(
    uint8_t num_buckets,
    base::span<const uint8_t> deployment_id) {
  AthmKeyMaterial key_material =
      athm_key_gen(num_buckets, base::SpanToRustSlice(deployment_id));
  if (key_material.status != AthmStatus::Ok) {
    return std::nullopt;
  }
  return AthmTestIssuer(base::ToVector(key_material.params),
                        base::ToVector(key_material.private_key),
                        base::ToVector(key_material.public_key),
                        base::ToVector(key_material.public_key_proof));
}

AthmTestIssuer::AthmTestIssuer(std::vector<uint8_t> params,
                               std::vector<uint8_t> private_key,
                               std::vector<uint8_t> public_key,
                               std::vector<uint8_t> public_key_proof)
    : params_(std::move(params)),
      private_key_(std::move(private_key)),
      public_key_(std::move(public_key)),
      public_key_proof_(std::move(public_key_proof)),
      key_id_(crypto::hash::Sha256(public_key_)) {}

AthmTestIssuer::AthmTestIssuer(AthmTestIssuer&&) = default;
AthmTestIssuer& AthmTestIssuer::operator=(AthmTestIssuer&&) = default;
AthmTestIssuer::~AthmTestIssuer() = default;

std::optional<std::vector<uint8_t>> AthmTestIssuer::Issue(
    base::span<const uint8_t> request,
    uint8_t hidden_metadata) const {
  AthmBytesResult result = athm_issue(
      base::SpanToRustSlice(private_key_), base::SpanToRustSlice(public_key_),
      base::SpanToRustSlice(request), hidden_metadata,
      base::SpanToRustSlice(params_));
  if (result.status != AthmStatus::Ok) {
    return std::nullopt;
  }
  return base::ToVector(result.bytes);
}

std::optional<std::vector<std::vector<uint8_t>>> AthmTestIssuer::BatchIssue(
    const std::vector<std::vector<uint8_t>>& requests,
    uint8_t hidden_metadata) const {
  if (requests.empty()) {
    return std::nullopt;
  }
  std::vector<std::vector<uint8_t>> responses;
  responses.reserve(requests.size());
  for (const auto& request : requests) {
    anonymous_tokens::AthmTokenRequest unmarshaled_request;
    absl::Status status = anonymous_tokens::UnmarshalAthmTokenRequest(
        base::as_string_view(request), &unmarshaled_request);
    if (!status.ok() ||
        unmarshaled_request.token_type !=
            PrivateVerificationTokensParameters::kAthmTokenType ||
        unmarshaled_request.truncated_issuer_key_id != truncated_key_id()) {
      return std::nullopt;
    }
    std::optional<std::vector<uint8_t>> response =
        Issue(base::as_byte_span(unmarshaled_request.encoded_request),
              hidden_metadata);
    if (!response.has_value()) {
      return std::nullopt;
    }
    responses.push_back(std::move(*response));
  }
  return responses;
}

std::optional<std::string> AthmTestIssuer::BatchIssue(
    std::string_view request_body,
    uint8_t hidden_metadata) const {
  std::optional<PrivateVerificationTokensParameters> params =
      GetParametersForVersion(1);
  if (!params.has_value()) {
    return std::nullopt;
  }

  if (request_body.empty() ||
      request_body.size() % params->single_request_size != 0) {
    return std::nullopt;
  }

  const size_t batch_size = request_body.size() / params->single_request_size;
  std::vector<std::vector<uint8_t>> requests;
  requests.reserve(batch_size);
  for (size_t i = 0; i < batch_size; ++i) {
    base::span<const uint8_t> single_req =
        base::as_byte_span(request_body)
            .subspan(i * params->single_request_size,
                     params->single_request_size);
    requests.push_back(base::ToVector(single_req));
  }

  std::optional<std::vector<std::vector<uint8_t>>> responses =
      BatchIssue(requests, hidden_metadata);
  if (!responses.has_value()) {
    return std::nullopt;
  }

  std::string response_body;
  for (const auto& token_resp : *responses) {
    response_body.append(base::as_string_view(token_resp));
  }
  return response_body;
}

std::optional<uint8_t> AthmTestIssuer::Verify(
    base::span<const uint8_t> token) const {
  AthmVerifyResult result =
      athm_verify(base::SpanToRustSlice(private_key_),
                  base::SpanToRustSlice(token), base::SpanToRustSlice(params_));
  if (result.status != AthmStatus::Ok) {
    return std::nullopt;
  }
  return result.metadata;
}

std::optional<uint8_t> AthmTestIssuer::VerifyWithCheck(
    base::span<const uint8_t> marshaled_token) const {
  anonymous_tokens::AthmToken token;
  absl::Status status = anonymous_tokens::UnmarshalAthmToken(
      base::as_string_view(marshaled_token), &token);
  if (!status.ok() ||
      token.token_type != PrivateVerificationTokensParameters::kAthmTokenType ||
      base::as_byte_span(token.issuer_key_id) != base::as_byte_span(key_id_)) {
    return std::nullopt;
  }
  return Verify(base::as_byte_span(token.token));
}

// --- AthmTestClient ---

// static
std::optional<AthmTestClient> AthmTestClient::Create(
    base::span<const uint8_t> public_key,
    base::span<const uint8_t> public_key_proof,
    uint8_t num_buckets,
    base::span<const uint8_t> deployment_id) {
  AthmClientParams params =
      athm_client_params(num_buckets, base::SpanToRustSlice(deployment_id));
  if (params.status != AthmStatus::Ok) {
    return std::nullopt;
  }

  return AthmTestClient(base::ToVector(public_key),
                        base::ToVector(public_key_proof),
                        base::ToVector(params.params));
}

AthmTestClient::AthmTestClient(std::vector<uint8_t> public_key,
                               std::vector<uint8_t> public_key_proof,
                               std::vector<uint8_t> params)
    : public_key_(std::move(public_key)),
      public_key_proof_(std::move(public_key_proof)),
      params_(std::move(params)) {}

AthmTestClient::~AthmTestClient() = default;
AthmTestClient::AthmTestClient(const AthmTestClient&) = default;
AthmTestClient& AthmTestClient::operator=(const AthmTestClient&) = default;
AthmTestClient::AthmTestClient(AthmTestClient&&) = default;
AthmTestClient& AthmTestClient::operator=(AthmTestClient&&) = default;

std::optional<AthmTestClient::ClientRequest>
AthmTestClient::CreateClientRequest() const {
  AthmClientRequest bridge_result = athm_client_request(
      base::SpanToRustSlice(public_key_),
      base::SpanToRustSlice(public_key_proof_), base::SpanToRustSlice(params_));
  if (bridge_result.status != AthmStatus::Ok) {
    return std::nullopt;
  }
  return ClientRequest{.context = base::ToVector(bridge_result.context),
                       .request = base::ToVector(bridge_result.request)};
}

std::optional<std::vector<uint8_t>> AthmTestClient::FinalizeToken(
    const ClientRequest& client_request,
    base::span<const uint8_t> response) const {
  AthmBytesResult result = athm_client_finalize(
      base::SpanToRustSlice(client_request.context),
      base::SpanToRustSlice(public_key_),
      base::SpanToRustSlice(client_request.request),
      base::SpanToRustSlice(response), base::SpanToRustSlice(params_));
  if (result.status != AthmStatus::Ok) {
    return std::nullopt;
  }
  return base::ToVector(result.bytes);
}

}  // namespace private_verification_tokens
