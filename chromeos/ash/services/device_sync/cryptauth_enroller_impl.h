// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_enroller.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"

namespace ash {

namespace multidevice {
class SecureMessageDelegate;
}

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;

// Implementation of CryptAuthEnroller to perform enrollment in two steps:
// 1. SetupEnrollment:
//     Obtain a session public key from CryptAuth used to encrypt enrollment
//     data. Generate an ephemeral public key and derive a session symmetric
//     key.
// 2. FinishEnrollment:
//     Encrypt the enrollment data with the session symmetric key, and send the
//     payload and device's public key to CryptAuth.
class CryptAuthEnrollerImpl : public CryptAuthEnroller {
 public:
  // |client_factory| creates CryptAuthClient instances for making API calls.
  // |crypto_delegate| is responsible for SecureMessage operations.
  CryptAuthEnrollerImpl(CryptAuthClientFactory* client_factory,
                        std::unique_ptr<multidevice::SecureMessageDelegate>
                            secure_message_delegate);

  CryptAuthEnrollerImpl(const CryptAuthEnrollerImpl&) = delete;
  CryptAuthEnrollerImpl& operator=(const CryptAuthEnrollerImpl&) = delete;

  ~CryptAuthEnrollerImpl() override;

  // CryptAuthEnroller:
  void Enroll(const std::string& user_public_key,
              const std::string& user_private_key,
              const cryptauth::GcmDeviceInfo& device_info,
              cryptauth::InvocationReason invocation_reason,
              EnrollmentFinishedCallback callback) override;

 private:
  // Callbacks for SetupEnrollment.
  void OnSetupEnrollmentSuccess(
      const cryptauth::SetupEnrollmentResponse& response);
  void OnSetupEnrollmentFailure(NetworkRequestError error);

  // Callbacks for FinishEnrollment.
  void OnFinishEnrollmentSuccess(
      const cryptauth::FinishEnrollmentResponse& response);
  void OnFinishEnrollmentFailure(NetworkRequestError error);

  // Callbacks for SecureMessageDelegate operations.
  void OnKeyPairGenerated(const std::string& public_key,
                          const std::string& private_key);
  void OnKeyDerived(const std::string& symmetric_key);
  void OnInnerSecureMessageCreated(const std::string& inner_message);
  void OnOuterSecureMessageCreated(const std::string& outer_message);

  // Creates the CryptAuthClient instances to make API requests.
  raw_ptr<CryptAuthClientFactory> client_factory_;

  // Handles SecureMessage operations.
  std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate_;

  // The CryptAuthClient for the latest request.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  // The ephemeral key-pair generated for a single enrollment.
  std::string session_public_key_;
  std::string session_private_key_;

  // The user's persistent key-pair identifying the local device.
  std::string user_public_key_;
  std::string user_private_key_;

  // Contains information of the device to enroll.
  cryptauth::GcmDeviceInfo device_info_;

  // The reason telling the server why the enrollment happened.
  cryptauth::InvocationReason invocation_reason_;

  // The setup information returned from the SetupEnrollment API call.
  cryptauth::SetupEnrollmentInfo setup_info_;

  // Callback invoked when the enrollment is done.
  EnrollmentFinishedCallback callback_;

  // True if Enroll() has been called.
  bool enroll_called_ = false;

  // The derived ephemeral symmetric key.
  std::string symmetric_key_;

  base::WeakPtrFactory<CryptAuthEnrollerImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_IMPL_H_
