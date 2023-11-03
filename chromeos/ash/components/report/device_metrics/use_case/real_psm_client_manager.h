// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_REAL_PSM_CLIENT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_REAL_PSM_CLIENT_MANAGER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/report/device_metrics/use_case/psm_client_manager.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::report::device_metrics {

// Real implementation for creating the PSM client manager.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
    RealPsmClientManagerDelegate
    : public device_metrics::PsmClientManager::Delegate {
 public:
  RealPsmClientManagerDelegate();
  RealPsmClientManagerDelegate(const RealPsmClientManagerDelegate&) = delete;
  RealPsmClientManagerDelegate& operator=(const RealPsmClientManagerDelegate&) =
      delete;
  ~RealPsmClientManagerDelegate() override;

  // PsmClientManager::Delegate:
  void SetPsmRlweClient(
      private_membership::rlwe::RlweUseCase use_case,
      const std::vector<private_membership::rlwe::RlwePlaintextId>&
          plaintext_ids) override;
  private_membership::rlwe::PrivateMembershipRlweClient* GetPsmRlweClient()
      override;
  rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  Create(private_membership::rlwe::RlweUseCase use_case,
         const std::vector<private_membership::rlwe::RlwePlaintextId>&
             plaintext_ids) override;
  rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreateForTesting(private_membership::rlwe::RlweUseCase use_case,
                   const std::vector<private_membership::rlwe::RlwePlaintextId>&
                       plaintext_ids,
                   std::string_view ec_cipher_key,
                   std::string_view seed) override;
  rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweOprfRequest>
  CreateOprfRequest() override;
  rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweQueryRequest>
  CreateQueryRequest(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response) override;
  rlwe::StatusOr<private_membership::rlwe::RlweMembershipResponses>
  ProcessQueryResponse(
      const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
          query_response) override;

 private:
  // Used to generate the request body of Oprf and Query requests.
  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
};

}  // namespace ash::report::device_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_REAL_PSM_CLIENT_MANAGER_H_
