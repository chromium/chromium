// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/fake_psm_claim_verifier.h"

namespace ash::carrier_lock {

void FakePsmClaimVerifier::CheckPsmClaim(std::string serial,
                                         std::string manufacturer,
                                         std::string model,
                                         Callback callback) {
  std::move(callback).Run(result_);
}

bool FakePsmClaimVerifier::GetMembership() {
  return member_;
}

void FakePsmClaimVerifier::SetMemberAndResult(bool member, Result result) {
  member_ = member;
  result_ = result;
}

}  // namespace ash::carrier_lock
