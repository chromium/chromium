// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_V2_ENROLLER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_V2_ENROLLER_H_

#include "base/callback.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"
#include "chromeos/services/device_sync/cryptauth_v2_enroller.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

// Implementation of CryptAuthV2Enroller for use with tests.
class FakeCryptAuthV2Enroller : public CryptAuthV2Enroller {
 public:
  FakeCryptAuthV2Enroller();
  ~FakeCryptAuthV2Enroller() override;

  // Invokes CryptAuthV2Enroller::OnAttemptFinished(|enrollment_result|).
  void FinishAttempt(const CryptAuthEnrollmentResult& enrollment_result);

  // Returns true if CryptAuthV2Enroller::Enroll() was called.
  bool was_enroll_called() { return was_enroll_called_; }

  // Returns ClientMetadata passed to OnAttemptStarted(). If OnAttemptStarted()
  // has not been called, null is returned.
  base::Optional<cryptauthv2::ClientMetadata> client_metadata() {
    return client_metadata_;
  }

  // Returns ClientAppMetadata passed to OnAttemptStarted(). If
  // OnAttemptStarted() has not been called, null is returned.
  base::Optional<cryptauthv2::ClientAppMetadata> client_app_metadata() {
    return client_app_metadata_;
  }

  // Returns the optional PolicyReference passed to OnAttemptedStarted(). If
  // OnAttemptStarted() has not been called, null is returned. Note: The return
  // type is a double optional; tread carefully.
  base::Optional<base::Optional<cryptauthv2::PolicyReference>>
  client_directive_policy_reference() {
    return client_directive_policy_reference_;
  }

 private:
  // CryptAuthV2Enroller:
  void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      const base::Optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) override;

  base::Optional<cryptauthv2::ClientMetadata> client_metadata_;
  base::Optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;
  base::Optional<base::Optional<cryptauthv2::PolicyReference>>
      client_directive_policy_reference_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_V2_ENROLLER_H_
