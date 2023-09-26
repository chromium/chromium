// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PSM_CLAIM_VERIFIER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PSM_CLAIM_VERIFIER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/carrier_lock/psm_claim_verifier.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ash::carrier_lock {

// This class communicates with the Private Set Membership service to check
// whether the cellular modem should be locked to particular network carrier.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    PsmClaimVerifierImpl : public PsmClaimVerifier {
 public:
  explicit PsmClaimVerifierImpl(scoped_refptr<network::SharedURLLoaderFactory>);
  PsmClaimVerifierImpl() = delete;
  ~PsmClaimVerifierImpl() override;

  // PsmClaimVerifier
  void CheckPsmClaim(std::string serial,
                     std::string manufacturer,
                     std::string model,
                     Callback callback) override;
  bool GetMembership() override;

 private:
  void SetupUrlLoader(std::string& request, const char* url);
  void SendOprfRequest();
  void OnCheckMembershipQueryDone(std::unique_ptr<std::string> response_body);
  void OnCheckMembershipOprfDone(std::unique_ptr<std::string> response_body);

  void ReturnError(Result error);
  void ReturnSuccess();

  private_membership::MembershipResponse membership_response_;
  Callback claim_callback_;
  std::string api_key_;

  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_client_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<PsmClaimVerifierImpl> weak_ptr_factory_{this};
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PSM_CLAIM_VERIFIER_IMPL_H_
