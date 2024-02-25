// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_PSM_CLAIM_VERIFIER_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_PSM_CLAIM_VERIFIER_H_

#include "chromeos/ash/components/carrier_lock/psm_claim_verifier.h"

namespace ash::carrier_lock {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    FakePsmClaimVerifier : public PsmClaimVerifier {
 public:
  FakePsmClaimVerifier() = default;
  ~FakePsmClaimVerifier() override = default;

  // PsmClaimVerifier
  void CheckPsmClaim(std::string serial,
                     std::string manufacturer,
                     std::string model,
                     Callback callback) override;
  bool GetMembership() override;
  void SetMemberAndResult(bool member, Result result);

 private:
  Result result_;
  bool member_;
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_PSM_CLAIM_VERIFIER_H_
