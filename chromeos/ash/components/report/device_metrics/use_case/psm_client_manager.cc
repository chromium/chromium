// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/use_case/psm_client_manager.h"

#include <utility>

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

PsmClientManager::Delegate::Delegate() = default;

PsmClientManager::Delegate::~Delegate() = default;

PsmClientManager::PsmClientManager(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

PsmClientManager::~PsmClientManager() = default;

void PsmClientManager::SetPsmRlweClient(
    psm_rlwe::RlweUseCase psm_use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  DCHECK(!plaintext_ids.empty());
  return delegate_->SetPsmRlweClient(psm_use_case, plaintext_ids);
}

psm_rlwe::PrivateMembershipRlweClient* PsmClientManager::GetPsmRlweClient() {
  return delegate_->GetPsmRlweClient();
}

rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
PsmClientManager::Create(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  DCHECK(!plaintext_ids.empty());
  return delegate_->Create(use_case, plaintext_ids);
}

rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
PsmClientManager::CreateForTesting(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids,
    std::string_view ec_cipher_key,
    std::string_view seed) {
  return delegate_->CreateForTesting(use_case, plaintext_ids, ec_cipher_key,
                                     seed);
}

rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
PsmClientManager::CreateOprfRequest() {
  return delegate_->CreateOprfRequest();
}

rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
PsmClientManager::CreateQueryRequest(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response) {
  return delegate_->CreateQueryRequest(oprf_response);
}

rlwe::StatusOr<psm_rlwe::RlweMembershipResponses>
PsmClientManager::ProcessQueryResponse(
    const psm_rlwe::PrivateMembershipRlweQueryResponse& query_response) {
  return delegate_->ProcessQueryResponse(query_response);
}

}  // namespace ash::report::device_metrics
