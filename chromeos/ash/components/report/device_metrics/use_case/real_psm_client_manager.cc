// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/use_case/real_psm_client_manager.h"

#include <utility>

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

RealPsmClientManagerDelegate::RealPsmClientManagerDelegate() = default;

RealPsmClientManagerDelegate::~RealPsmClientManagerDelegate() = default;

void RealPsmClientManagerDelegate::SetPsmRlweClient(
    psm_rlwe::RlweUseCase psm_use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  DCHECK(!plaintext_ids.empty());
  auto status_or_client = Create(psm_use_case, plaintext_ids);
  if (!status_or_client.ok()) {
    LOG(ERROR) << "Failed to initialize PSM RLWE client.";
    return;
  }

  psm_rlwe_client_ = std::move(status_or_client.value());
}

psm_rlwe::PrivateMembershipRlweClient*
RealPsmClientManagerDelegate::GetPsmRlweClient() {
  DCHECK(psm_rlwe_client_);
  return psm_rlwe_client_.get();
}

rlwe::StatusOr<
    std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
RealPsmClientManagerDelegate::Create(
    private_membership::rlwe::RlweUseCase use_case,
    const std::vector<private_membership::rlwe::RlwePlaintextId>&
        plaintext_ids) {
  return private_membership::rlwe::PrivateMembershipRlweClient::Create(
      use_case, plaintext_ids);
}

rlwe::StatusOr<
    std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
RealPsmClientManagerDelegate::CreateForTesting(
    private_membership::rlwe::RlweUseCase use_case,
    const std::vector<private_membership::rlwe::RlwePlaintextId>& plaintext_ids,
    std::string_view ec_cipher_key,
    std::string_view seed) {
  return private_membership::rlwe::PrivateMembershipRlweClient::
      CreateForTesting(use_case, plaintext_ids, ec_cipher_key, seed);
}

rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweOprfRequest>
RealPsmClientManagerDelegate::CreateOprfRequest() {
  DCHECK(psm_rlwe_client_);
  return psm_rlwe_client_->CreateOprfRequest();
}

rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweQueryRequest>
RealPsmClientManagerDelegate::CreateQueryRequest(
    const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
        oprf_response) {
  DCHECK(psm_rlwe_client_);
  return psm_rlwe_client_->CreateQueryRequest(oprf_response);
}

rlwe::StatusOr<private_membership::rlwe::RlweMembershipResponses>
RealPsmClientManagerDelegate::ProcessQueryResponse(
    const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
        query_response) {
  DCHECK(psm_rlwe_client_);
  return psm_rlwe_client_->ProcessQueryResponse(query_response);
}

}  // namespace ash::report::device_metrics
