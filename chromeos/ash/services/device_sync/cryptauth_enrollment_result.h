// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_

#include <optional>
#include <ostream>

#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

namespace ash {

namespace device_sync {

// Information about the result of a CryptAuth v2 Enrollment attempt.
class CryptAuthEnrollmentResult {
 public:
  // Enum class to denote the result of a CryptAuth v2 Enrollment attempt.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class ResultCode {
    // Successfully synced but no new keys were requested by CryptAuth, so no
    // EnrollKeysRequest was made.
    kSuccessNoNewKeysNeeded = 0,
    // Successfully synced and enrolled new key(s) with CryptAuth.
    kSuccessNewKeysEnrolled = 1,
    // During SyncKeys API call, request could not be completed because the
    // device is offline or has issues sending the HTTP request.
    kErrorSyncKeysApiCallOffline = 2,
    // During SyncKeys API call, server endpoint could not be found.
    kErrorSyncKeysApiCallEndpointNotFound = 3,
    // During SyncKeys API call, authentication error contacting back-end.
    kErrorSyncKeysApiCallAuthenticationError = 4,
    // During SyncKeys API call, network request was invalid.
    kErrorSyncKeysApiCallBadRequest = 5,
    // During SyncKeys API call, the server responded but the response was not
    // formatted correctly.
    kErrorSyncKeysApiCallResponseMalformed = 6,
    // During SyncKeys API call, internal server error.
    kErrorSyncKeysApiCallInternalServerError = 7,
    // During SyncKeys API call, unknown network request error.
    kErrorSyncKeysApiCallUnknownError = 8,
    // During EnrollKeys API call, request could not be completed because the
    // device is offline or has issues sending the HTTP request.
    kErrorEnrollKeysApiCallOffline = 9,
    // During EnrollKeys API call, server endpoint could not be found.
    kErrorEnrollKeysApiCallEndpointNotFound = 10,
    // During EnrollKeys API call, authentication error contacting back-end.
    kErrorEnrollKeysApiCallAuthenticationError = 11,
    // During EnrollKeys API call, network request was invalid.
    kErrorEnrollKeysApiCallBadRequest = 12,
    // During EnrollKeys API call, the server responded but the response was not
    // formatted correctly.
    kErrorEnrollKeysApiCallResponseMalformed = 13,
    // During EnrollKeys API call, internal server error.
    kErrorEnrollKeysApiCallInternalServerError = 14,
    // During EnrollKeys API call, unknown network request error.
    kErrorEnrollKeysApiCallUnknownError = 15,
    // The CryptAuth server indicated via SyncKeysResponse::server_status that
    // it was overloaded and did not process the SyncKeysRequest.
    kErrorCryptAuthServerOverloaded = 16,
    // The SyncKeysResponse from CryptAuth is missing a random_session_id.
    kErrorSyncKeysResponseMissingRandomSessionId = 17,
    // The SyncKeysResponse from CryptAuth is missing a client_directive or the
    // parameters are invalid.
    kErrorSyncKeysResponseInvalidClientDirective = 18,
    // The number of SyncSingleKeyResponses does not agree with the number of
    // SyncSingleKeyRequests.
    kErrorWrongNumberOfSyncSingleKeyResponses = 19,
    // The size of a SyncSingleKeyResponse::key_actions list does not agree with
    // the size of the corresponding SyncSingleKeyRequest::key_handles.
    kErrorWrongNumberOfKeyActions = 20,
    // An integer provided in SyncSingleKeyResponse::key_actions does not
    // correspond to a known KeyAction enum value.
    kErrorInvalidKeyActionEnumValue = 21,
    // The SyncSingleKeyResponse::key_actions denote more than one active key.
    kErrorKeyActionsSpecifyMultipleActiveKeys = 22,
    // The SyncSingleKeyResponse::key_actions fail to specify an active key.
    kErrorKeyActionsDoNotSpecifyAnActiveKey = 23,
    // KeyCreation instructions specify an unsupported KeyType.
    kErrorKeyCreationKeyTypeNotSupported = 24,
    // Invalid key-creation instructions for user key pair. It must be P256 and
    // active.
    kErrorUserKeyPairCreationInstructionsInvalid = 25,
    // Cannot create a symmetric key without the server's Diffie-Hellman key.
    kErrorSymmetricKeyCreationMissingServerDiffieHellman = 26,
    // Failed to compute at least one key proof.
    kErrorKeyProofComputationFailed = 27,
    // The enroller timed out waiting for response from the SyncKeys API call.
    kErrorTimeoutWaitingForSyncKeysResponse = 28,
    // The enroller timed out waiting for new keys to be created.
    kErrorTimeoutWaitingForKeyCreation = 29,
    // The enroller timed out waiting for key proofs to be computed.
    kErrorTimeoutWaitingForEnrollKeysResponse = 30,
    // Failed to register local device with GCM. This registration is required
    // in order for CryptAuth to send GCM messages to the local device,
    // requesting it to re-enroll or re-sync.
    kErrorGcmRegistrationFailed = 31,
    // Could not retrieve ClientAppMetadata from ClientAppMetadataProvider.
    kErrorClientAppMetadataFetchFailed = 32,
    // The enrollment manager timed out waiting for GCM registration.
    kErrorTimeoutWaitingForGcmRegistration = 33,
    // The enrollment manager timed out waiting for ClientAppMetadata.
    kErrorTimeoutWaitingForClientAppMetadata = 34,
    // Failed to create the user key pair to be enrolled.
    kErrorUserKeyPairCreationFailed = 35,
    // Failed to create the legacy authzen key to be enrolled.
    kErrorLegacyAuthzenKeyCreationFailed = 36,
    // Failed to create the DeviceSync:BetterTogether key to be enrolled.
    kErrorDeviceSyncBetterTogetherKeyCreationFailed = 37,
    // Used for UMA logs.
    kMaxValue = kErrorDeviceSyncBetterTogetherKeyCreationFailed
  };

  CryptAuthEnrollmentResult(
      ResultCode result_code,
      const std::optional<cryptauthv2::ClientDirective>& client_directive);
  CryptAuthEnrollmentResult(const CryptAuthEnrollmentResult& other);

  ~CryptAuthEnrollmentResult();

  ResultCode result_code() const { return result_code_; }

  const std::optional<cryptauthv2::ClientDirective>& client_directive() const {
    return client_directive_;
  }

  bool IsSuccess() const;

  bool operator==(const CryptAuthEnrollmentResult& other) const;
  bool operator!=(const CryptAuthEnrollmentResult& other) const;

 private:
  ResultCode result_code_;
  std::optional<cryptauthv2::ClientDirective> client_directive_;
};

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthEnrollmentResult::ResultCode result_code);

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_
