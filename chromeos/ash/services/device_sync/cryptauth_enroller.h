// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"

namespace ash {

namespace device_sync {

// Interface for enrolling a device with CryptAuth.
class CryptAuthEnroller {
 public:
  virtual ~CryptAuthEnroller() {}

  // Enrolls the device:
  // |user_public_key|: The user's persistent public key identifying the device.
  // |user_private_key|: The corresponding private key to |user_public_key|.
  // |device_info|: Contains information about the local device. Note that the
  //     enroller may change fields in this proto before it is finally uploaded.
  // |invocation_reason|: The reason why the enrollment occurred.
  // |callback|: Called will be called with true if the enrollment
  //     succeeds and false otherwise.
  typedef base::OnceCallback<void(bool)> EnrollmentFinishedCallback;
  virtual void Enroll(const std::string& user_public_key,
                      const std::string& user_private_key,
                      const cryptauth::GcmDeviceInfo& device_info,
                      cryptauth::InvocationReason invocation_reason,
                      EnrollmentFinishedCallback callback) = 0;
};

// Interface for creating CryptAuthEnroller instances.
class CryptAuthEnrollerFactory {
 public:
  virtual ~CryptAuthEnrollerFactory() {}

  virtual std::unique_ptr<CryptAuthEnroller> CreateInstance() = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLER_H_
