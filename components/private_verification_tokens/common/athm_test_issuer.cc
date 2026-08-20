// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/athm_test_issuer.h"

#include <utility>

#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "crypto/hash.h"
#include "third_party/anonymous_tokens/src/anonymous_tokens/cpp/privacy_pass/athm_token_encodings_utils.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"

namespace private_verification_tokens {

// static
std::optional<AthmTestIssuer> AthmTestIssuer::Create(
    uint8_t num_buckets,
    base::span<const uint8_t> deployment_id) {
  auto key_material = AthmKeyMaterial::try_new(
      num_buckets, rs_std::SliceRef<const uint8_t>(deployment_id));
  if (!key_material.has_value()) {
    return std::nullopt;
  }
  return AthmTestIssuer(std::move(*key_material));
}

AthmTestIssuer::AthmTestIssuer(AthmKeyMaterial key_material)
    : key_material_(std::move(key_material)) {}

AthmTestIssuer::AthmTestIssuer(AthmTestIssuer&&) = default;
AthmTestIssuer& AthmTestIssuer::operator=(AthmTestIssuer&&) = default;
AthmTestIssuer::~AthmTestIssuer() = default;

std::optional<std::vector<uint8_t>> AthmTestIssuer::Issue(
    const TokenRequest& request,
    uint8_t hidden_metadata) const {
  auto result = key_material_.issue(request, hidden_metadata);
  if (!result.has_value()) {
    return std::nullopt;
  }
  return base::ToVector(*result);
}

std::optional<std::vector<uint8_t>> AthmTestIssuer::Issue(
    base::span<const uint8_t> request,
    uint8_t hidden_metadata) const {
  auto token_req =
      TokenRequest::decode(rs_std::SliceRef<const uint8_t>(request));
  if (!token_req.has_value()) {
    return std::nullopt;
  }
  return Issue(*token_req, hidden_metadata);
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
  auto result = key_material_.verify(rs_std::SliceRef<const uint8_t>(token));
  if (!result.has_value()) {
    return std::nullopt;
  }
  return *result;
}

std::optional<uint8_t> AthmTestIssuer::VerifyWithCheck(
    base::span<const uint8_t> marshaled_token) const {
  anonymous_tokens::AthmToken token;
  absl::Status status = anonymous_tokens::UnmarshalAthmToken(
      base::as_string_view(marshaled_token), &token);
  if (!status.ok() ||
      token.token_type != PrivateVerificationTokensParameters::kAthmTokenType ||
      base::as_byte_span(token.issuer_key_id) != base::as_byte_span(key_id())) {
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
  auto params = AthmParameters::try_new(
      num_buckets, rs_std::SliceRef<const uint8_t>(deployment_id));
  if (!params.has_value()) {
    return std::nullopt;
  }

  return AthmTestClient(base::ToVector(public_key),
                        base::ToVector(public_key_proof), std::move(*params));
}

AthmTestClient::AthmTestClient(std::vector<uint8_t> public_key,
                               std::vector<uint8_t> public_key_proof,
                               AthmParameters params)
    : public_key_(std::move(public_key)),
      public_key_proof_(std::move(public_key_proof)),
      params_(std::move(params)) {}

AthmTestClient::~AthmTestClient() = default;
AthmTestClient::AthmTestClient(const AthmTestClient&) = default;
AthmTestClient& AthmTestClient::operator=(const AthmTestClient&) = default;
AthmTestClient::AthmTestClient(AthmTestClient&&) = default;
AthmTestClient& AthmTestClient::operator=(AthmTestClient&&) = default;

std::optional<AthmClientRequest> AthmTestClient::CreateClientRequest() const {
  auto bridge_result = AthmClientRequest::try_new(
      rs_std::SliceRef<const uint8_t>(public_key_),
      rs_std::SliceRef<const uint8_t>(public_key_proof_), params_);
  if (!bridge_result.has_value()) {
    return std::nullopt;
  }
  return std::move(*bridge_result);
}

std::optional<std::vector<uint8_t>> AthmTestClient::FinalizeToken(
    const AthmClientRequest& client_request,
    base::span<const uint8_t> response) const {
  auto result = client_request.finalize(
      rs_std::SliceRef<const uint8_t>(public_key_),
      rs_std::SliceRef<const uint8_t>(response), params_);
  if (!result.has_value()) {
    return std::nullopt;
  }
  return base::ToVector(*result);
}

}  // namespace private_verification_tokens
