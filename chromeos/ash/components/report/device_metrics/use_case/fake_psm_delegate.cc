// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/use_case/fake_psm_delegate.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::report::device_metrics {

FakePsmDelegate::FakePsmDelegate(
    const std::string& ec_cipher_key,
    const std::string& seed,
    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids)
    : ec_cipher_key_(ec_cipher_key),
      seed_(seed),
      plaintext_ids_(plaintext_ids) {}

FakePsmDelegate::~FakePsmDelegate() = default;

rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
FakePsmDelegate::CreatePsmClient(
    psm_rlwe::RlweUseCase use_case,
    const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) {
  return psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
      use_case, plaintext_ids_, ec_cipher_key_, seed_);
}

}  // namespace ash::report::device_metrics
