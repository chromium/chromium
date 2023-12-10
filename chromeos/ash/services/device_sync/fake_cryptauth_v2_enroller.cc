// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_v2_enroller.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {

namespace device_sync {

FakeCryptAuthV2Enroller::FakeCryptAuthV2Enroller() = default;

FakeCryptAuthV2Enroller::~FakeCryptAuthV2Enroller() = default;

void FakeCryptAuthV2Enroller::FinishAttempt(
    const CryptAuthEnrollmentResult& enrollment_result) {
  OnAttemptFinished(enrollment_result);
}

void FakeCryptAuthV2Enroller::OnAttemptStarted(
    const cryptauthv2::ClientMetadata& client_metadata,
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    const std::optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  client_metadata_ = client_metadata;
  client_app_metadata_ = client_app_metadata;
  client_directive_policy_reference_ = client_directive_policy_reference;
}

}  // namespace device_sync

}  // namespace ash
