// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_PSM_CLIENT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_PSM_CLIENT_MANAGER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::report::device_metrics {

// Create a delegate which can be used to create stubs in unit tests.
// Stub via. delegate is required for testing the entire check membership flow.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_REPORT) PsmClientManager {
 public:
  // Delegate interface performs work on behalf of the PsmClientManager
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Asks to set the PSM RLWE client member variable.
    virtual void SetPsmRlweClient(
        private_membership::rlwe::RlweUseCase use_case,
        const std::vector<private_membership::rlwe::RlwePlaintextId>&
            plaintext_ids) = 0;

    // Asks to get the PSM RLWE client member variable.
    virtual private_membership::rlwe::PrivateMembershipRlweClient*
    GetPsmRlweClient() = 0;

    // Asks to create a client for the Private Membership RLWE protocol.
    virtual rlwe::StatusOr<
        std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
    Create(private_membership::rlwe::RlweUseCase use_case,
           const std::vector<private_membership::rlwe::RlwePlaintextId>&
               plaintext_ids) = 0;

    // Asks to create a client for testing the Private Membership RLWE protocol.
    virtual rlwe::StatusOr<
        std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
    CreateForTesting(
        private_membership::rlwe::RlweUseCase use_case,
        const std::vector<private_membership::rlwe::RlwePlaintextId>&
            plaintext_ids,
        std::string_view ec_cipher_key,
        std::string_view seed) = 0;

    // Asks to create a request proto for the first phase of the protocol.
    virtual rlwe::StatusOr<
        private_membership::rlwe::PrivateMembershipRlweOprfRequest>
    CreateOprfRequest() = 0;

    // Asks to create a request proto for the second phase of the protocol.
    virtual rlwe::StatusOr<
        private_membership::rlwe::PrivateMembershipRlweQueryRequest>
    CreateQueryRequest(
        const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
            oprf_response) = 0;

    // Asks to process the query response from the server and return the
    // membership response map.
    virtual rlwe::StatusOr<private_membership::rlwe::RlweMembershipResponses>
    ProcessQueryResponse(
        const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
            query_response) = 0;
  };

  explicit PsmClientManager(std::unique_ptr<Delegate> delegate);
  PsmClientManager(const PsmClientManager&) = delete;
  PsmClientManager& operator=(const PsmClientManager&) = delete;
  virtual ~PsmClientManager();

  // Generate the PSM RLWE client used to create OPRF and Query request bodies.
  void SetPsmRlweClient(
      private_membership::rlwe::RlweUseCase use_case,
      const std::vector<private_membership::rlwe::RlwePlaintextId>&
          plaintext_ids);

  // Return address of |psm_rlwe_client_| unique pointer, or null if not set.
  private_membership::rlwe::PrivateMembershipRlweClient* GetPsmRlweClient();

  // Create a client for the Private Membership RLWE protocol.
  rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  Create(private_membership::rlwe::RlweUseCase use_case,
         const std::vector<private_membership::rlwe::RlwePlaintextId>&
             plaintext_ids);

  // Create a client for testing the Private Membership RLWE protocol.
  rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreateForTesting(private_membership::rlwe::RlweUseCase use_case,
                   const std::vector<private_membership::rlwe::RlwePlaintextId>&
                       plaintext_ids,
                   std::string_view ec_cipher_key,
                   std::string_view seed);

  // Create a request proto for the first phase of the protocol.
  rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweOprfRequest>
  CreateOprfRequest();

  // Create a request proto for the second phase of the protocol.
  rlwe::StatusOr<private_membership::rlwe::PrivateMembershipRlweQueryRequest>
  CreateQueryRequest(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response);

  // Process the query response from the server and return the membership
  // response map.
  rlwe::StatusOr<private_membership::rlwe::RlweMembershipResponses>
  ProcessQueryResponse(
      const private_membership::rlwe::PrivateMembershipRlweQueryResponse&
          query_response);

 private:
  std::unique_ptr<Delegate> delegate_;
};

}  // namespace ash::report::device_metrics

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_DEVICE_METRICS_USE_CASE_PSM_CLIENT_MANAGER_H_
