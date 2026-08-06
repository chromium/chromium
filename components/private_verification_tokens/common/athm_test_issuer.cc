// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/athm_test_issuer.h"

#include <utility>

#include "base/containers/span_rust.h"
#include "base/containers/to_vector.h"
#include "components/private_verification_tokens/common/athm_ffi.rs.h"

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
      public_key_proof_(std::move(public_key_proof)) {}

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
