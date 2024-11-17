// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_STUB_PSM_CLIENT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_STUB_PSM_CLIENT_MANAGER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/report/device_metrics/use_case/psm_client_manager.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::report::device_metrics {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT)
    StubPsmClientManagerDelegate
    : public device_metrics::PsmClientManager::Delegate {
 public:
  StubPsmClientManagerDelegate();
  StubPsmClientManagerDelegate(const StubPsmClientManagerDelegate&) = delete;
  StubPsmClientManagerDelegate& operator=(const StubPsmClientManagerDelegate&) =
      delete;
  ~StubPsmClientManagerDelegate() override;

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

  // Provide stub methods to be used in unit tests.
  void set_ec_cipher_key(std::string_view ec_cipher_key);

  void set_seed(std::string_view seed);

  void set_oprf_request(
      const private_membership::rlwe::PrivateMembershipRlweOprfRequest&
          oprf_request);

  void set_query_request(
      const private_membership::rlwe::PrivateMembershipRlweQueryRequest&
          query_request);

  void set_membership_responses(
      const private_membership::rlwe::RlweMembershipResponses&
          membership_responses);

 private:
  // Parameters that are set to generate PsmRlweClient object.
  // Default values are set in constructor but can be updated in unit tests
  // using set methods.
  std::string ec_cipher_key_;
  std::string seed_;

  // Results to return from PsmClientManager methods.
  rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweOprfRequest>
      oprf_request_;
  rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweQueryRequest>
      query_request_;
  rlwe::StatusOr<private_membership::rlwe::RlweMembershipResponses>
      membership_responses_;

  // Stores the test psm rlwe client used to generate the Oprf & Query requests.
  // Operations of this class no-op in unit tests.
  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
};

}  // namespace ash::report::device_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_STUB_PSM_CLIENT_MANAGER_H_
