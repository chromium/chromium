// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_v2_enroller.h"

#include <utility>

namespace chromeos {

namespace device_sync {

CryptAuthV2Enroller::CryptAuthV2Enroller() = default;

CryptAuthV2Enroller::~CryptAuthV2Enroller() = default;

void CryptAuthV2Enroller::Enroll(
    const cryptauthv2::ClientMetadata& client_metadata,
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    const base::Optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference,
    EnrollmentAttemptFinishedCallback callback) {
  // Enforce that Enroll() can only be called once.
  DCHECK(!was_enroll_called_);
  was_enroll_called_ = true;

  callback_ = std::move(callback);

  OnAttemptStarted(client_metadata, client_app_metadata,
                   client_directive_policy_reference);
}

void CryptAuthV2Enroller::OnAttemptFinished(
    const CryptAuthEnrollmentResult& enrollment_result) {
  DCHECK(callback_);
  std::move(callback_).Run(enrollment_result);
}

}  // namespace device_sync

}  // namespace chromeos
