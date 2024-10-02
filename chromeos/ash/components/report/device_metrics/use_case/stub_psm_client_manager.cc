// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/use_case/stub_psm_client_manager.h"

#include <utility>

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

namespace {

// Default parameters used in generating the PsmRlweClient for test.
// Can be overridden using set methods when unit testing.
constexpr char kEcCipherKeyDefault[] = "XYZ";
constexpr char kSeedDefault[] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

}  // namespace

StubPsmClientManagerDelegate::StubPsmClientManagerDelegate()
    : ec_cipher_key_(kEcCipherKeyDefault), seed_(kSeedDefault) {}

StubPsmClientManagerDelegate::~StubPsmClientManagerDelegate() = default;

void StubPsmClientManagerDelegate::SetPsmRlweClient(
    psm_rlwe::RlweUseCase psm_use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  DCHECK(!plaintext_ids.empty());
  auto status_or_client =
      CreateForTesting(psm_use_case, plaintext_ids, ec_cipher_key_, seed_);
  if (!status_or_client.ok()) {
    LOG(ERROR) << "Failed to initialize PSM RLWE client.";
    return;
  }

  psm_rlwe_client_ = std::move(status_or_client.value());
}

psm_rlwe::PrivateMembershipRlweClient*
StubPsmClientManagerDelegate::GetPsmRlweClient() {
  DCHECK(psm_rlwe_client_);
  return psm_rlwe_client_.get();
}

rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
StubPsmClientManagerDelegate::Create(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  return psm_rlwe::PrivateMembershipRlweClient::Create(use_case, plaintext_ids);
}

rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
StubPsmClientManagerDelegate::CreateForTesting(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids,
    std::string_view ec_cipher_key,
    std::string_view seed) {
  return psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
      use_case, plaintext_ids, ec_cipher_key, seed);
}

rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
StubPsmClientManagerDelegate::CreateOprfRequest() {
  return oprf_request_;
}

rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
StubPsmClientManagerDelegate::CreateQueryRequest(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  return query_request_;
}

rlwe::StatusOr<psm_rlwe::RlweMembershipResponses>
StubPsmClientManagerDelegate::ProcessQueryResponse(
    const psm_rlwe::PrivateMembershipRlweQueryResponse& query_response) {
  return membership_responses_;
}

void StubPsmClientManagerDelegate::set_ec_cipher_key(
    std::string_view ec_cipher_key) {
  ec_cipher_key_ = ec_cipher_key;
}

void StubPsmClientManagerDelegate::set_seed(std::string_view seed) {
  seed_ = seed;
}

void StubPsmClientManagerDelegate::set_oprf_request(
    const psm_rlwe::PrivateMembershipRlweOprfRequest& oprf_request) {
  oprf_request_ = oprf_request;
}

void StubPsmClientManagerDelegate::set_query_request(
    const psm_rlwe::PrivateMembershipRlweQueryRequest& query_request) {
  query_request_ = query_request;
}

void StubPsmClientManagerDelegate::set_membership_responses(
    const psm_rlwe::RlweMembershipResponses& membership_responses) {
  membership_responses_ = membership_responses;
}

}  // namespace ash::report::device_metrics
