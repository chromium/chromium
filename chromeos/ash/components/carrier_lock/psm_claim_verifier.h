// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PSM_CLAIM_VERIFIER_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PSM_CLAIM_VERIFIER_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/carrier_lock/common.h"

namespace ash::carrier_lock {

// This class communicates with the Private Set Membership service to check
// whether the cellular modem should be locked to particular network carrier.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK) PsmClaimVerifier {
 public:
  PsmClaimVerifier() = default;
  virtual ~PsmClaimVerifier() = default;

  // Send a request to PSM server. Result is Success or error code.
  virtual void CheckPsmClaim(std::string serial,
                             std::string manufacturer,
                             std::string model,
                             Callback callback) = 0;

  // Returns true if the device belongs to Carrier Lock group.
  virtual bool GetMembership() = 0;

 protected:
  friend class PsmClaimVerifierTest;

  void set_testing(bool testing) { is_testing_ = testing; }
  bool is_testing() { return is_testing_; }
  bool is_testing_ = false;
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PSM_CLAIM_VERIFIER_H_
