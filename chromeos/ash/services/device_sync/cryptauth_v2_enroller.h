// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLER_H_

#include <optional>

#include "base/functional/callback.h"

namespace cryptauthv2 {
class ClientAppMetadata;
class ClientMetadata;
class PolicyReference;
}  // namespace cryptauthv2

namespace ash {

namespace device_sync {

class CryptAuthEnrollmentResult;

// Implements the client end of the CryptAuth v2 Enrollment protocol, which
// consists of two request/response interactions with the CryptAuth servers:
//
// 1a) SyncKeysRequest: Contains the names of key bundles used by us--the
//     client--as well as the handles and metadata of any existing keys in those
//     key bundles. General metadata about the local device, such as hardware
//     and feature support, is also included. Even if new key bundles are not
//     being enrolled and no metadata is being changed, the Enrollment protocol
//     requires periodic check-ins with the CryptAuth server.
//
// 1b) SyncKeysResponse: The response from CryptAuth includes instructions about
//     what existing keys should be active, inactive, or deleted altogether. It
//     also provides information about what new keys, if any, should be
//     generated and added to one of the key bundles listed in the request.
//     Aside from key instructions, a client directive is returned, which
//     provides parameters related to scheduling the next check-in with the
//     server.
//
// 2a) EnrollKeysRequest: The second request in the Enrollment protocol is only
//     necessary if the client needs to enroll new keys, as denoted in the
//     SyncKeysResponse. The request contains information such as the material
//     of the new public key (if it is an asymmetric key) and necessary proof
//     for verifying that we indeed possess the private or symmetric key.
//
// 2b) EnrollKeysResponse: We simply view this response as an indication that
//     the EnrollKeysRequest was successful.
//
// A CryptAuthV2Enroller object is designed to be used for only one Enroll()
// call. For a new Enrollment attempt, a new object should be created.
class CryptAuthV2Enroller {
 public:
  CryptAuthV2Enroller(const CryptAuthV2Enroller&) = delete;
  CryptAuthV2Enroller& operator=(const CryptAuthV2Enroller&) = delete;

  virtual ~CryptAuthV2Enroller();

  using EnrollmentAttemptFinishedCallback =
      base::OnceCallback<void(const CryptAuthEnrollmentResult&)>;

  // Starts the enrollment flow.
  // |client_metadata|: Information about the enrollment attempt--invocation
  //     reason, retry count, etc.--that is sent to CryptAuth in the
  //     SyncKeysRequest.
  // |client_app_metadata|: Information about the local device being
  //     enrolled--such as IDs, device hardware, and feature support--that is
  //     sent to CryptAuth in the SyncKeysRequest.
  // |client_directive_policy_reference|: The policy reference used to identify
  //     the last ClientDirective that was received in a SyncKeysResponse. If
  //     the client has never received a ClientDirective, then this is
  //     std::nullopt.
  // |callback|: Invoked when the enrollment attempt concludes, successfully or
  //     not. The CryptAuthEnrollmentResult provides information about the
  //     outcome of the enrollment attempt and possibly a new ClientDirective.
  void Enroll(const cryptauthv2::ClientMetadata& client_metadata,
              const cryptauthv2::ClientAppMetadata& client_app_metadata,
              const std::optional<cryptauthv2::PolicyReference>&
                  client_directive_policy_reference,
              EnrollmentAttemptFinishedCallback callback);

 protected:
  CryptAuthV2Enroller();

  virtual void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata,
      const std::optional<cryptauthv2::PolicyReference>&
          client_directive_policy_reference) = 0;

  void OnAttemptFinished(const CryptAuthEnrollmentResult& enrollment_result);

  EnrollmentAttemptFinishedCallback callback_;
  bool was_enroll_called_ = false;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_ENROLLER_H_
