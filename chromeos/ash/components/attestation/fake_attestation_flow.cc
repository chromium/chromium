// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/fake_attestation_flow.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"

namespace ash {
namespace attestation {

// This constructor passes |nullptr|s to the base class because we don't use
// server proxy in |AttestationFlowIntegrated|.
//
// TODO(b/232893759): Remove this transitional state along with the removal of
// |AttestationFlow|.
FakeAttestationFlow::FakeAttestationFlow(const std::string& certificate)
    : certificate_(certificate) {}

FakeAttestationFlow::~FakeAttestationFlow() = default;

void FakeAttestationFlow::GetCertificate(
    AttestationCertificateProfile /*certificate_profile*/,
    const AccountId& /*account_id*/,
    const std::string& /*request_origin*/,
    bool /*force_new_key*/,
    ::attestation::KeyType /*key_crypto_type*/,
    const std::string& /*key_name*/,
    const std::optional<
        AttestationFlow::CertProfileSpecificData>& /*profile_specific_data*/,
    CertificateCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     AttestationStatus::ATTESTATION_SUCCESS, certificate_));
}

}  // namespace attestation
}  // namespace ash
