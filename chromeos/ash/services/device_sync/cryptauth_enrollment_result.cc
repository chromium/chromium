// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_enrollment_result.h"

namespace ash {

namespace device_sync {

CryptAuthEnrollmentResult::CryptAuthEnrollmentResult(
    ResultCode result_code,
    const std::optional<cryptauthv2::ClientDirective>& client_directive)
    : result_code_(result_code), client_directive_(client_directive) {}

CryptAuthEnrollmentResult::CryptAuthEnrollmentResult(
    const CryptAuthEnrollmentResult& other) = default;

CryptAuthEnrollmentResult::~CryptAuthEnrollmentResult() = default;

bool CryptAuthEnrollmentResult::IsSuccess() const {
  return result_code_ == ResultCode::kSuccessNewKeysEnrolled ||
         result_code_ == ResultCode::kSuccessNoNewKeysNeeded;
}

bool CryptAuthEnrollmentResult::operator==(
    const CryptAuthEnrollmentResult& other) const {
  return result_code_ == other.result_code_ &&
         client_directive_.has_value() == other.client_directive_.has_value() &&
         (!client_directive_ ||
          client_directive_->SerializeAsString() ==
              other.client_directive_->SerializeAsString());
}

bool CryptAuthEnrollmentResult::operator!=(
    const CryptAuthEnrollmentResult& other) const {
  return !(*this == other);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthEnrollmentResult::ResultCode result_code) {
  using ResultCode = CryptAuthEnrollmentResult::ResultCode;

  switch (result_code) {
    case ResultCode::kSuccessNoNewKeysNeeded:
      stream << "[Success: No new keys needed]";
      break;
    case ResultCode::kSuccessNewKeysEnrolled:
      stream << "[Success: New keys enrolled]";
      break;
    case ResultCode::kErrorSyncKeysApiCallOffline:
      stream << "[SyncKeys API call failed: Offline]";
      break;
    case ResultCode::kErrorSyncKeysApiCallEndpointNotFound:
      stream << "[SyncKeys API call failed: Endpoint not found]";
      break;
    case ResultCode::kErrorSyncKeysApiCallAuthenticationError:
      stream << "[SyncKeys API call failed: Authentication error]";
      break;
    case ResultCode::kErrorSyncKeysApiCallBadRequest:
      stream << "[SyncKeys API call failed: Bad request]";
      break;
    case ResultCode::kErrorSyncKeysApiCallResponseMalformed:
      stream << "[SyncKeys API call failed: Response malformed]";
      break;
    case ResultCode::kErrorSyncKeysApiCallInternalServerError:
      stream << "[SyncKeys API call failed: Internal server error]";
      break;
    case ResultCode::kErrorSyncKeysApiCallUnknownError:
      stream << "[SyncKeys API call failed: Unknown error]";
      break;
    case ResultCode::kErrorEnrollKeysApiCallOffline:
      stream << "[EnrollKeys API call failed: Offline]";
      break;
    case ResultCode::kErrorEnrollKeysApiCallEndpointNotFound:
      stream << "[EnrollKeys API call failed: Endpoint not found]";
      break;
    case ResultCode::kErrorEnrollKeysApiCallAuthenticationError:
      stream << "[EnrollKeys API call failed: Authentication error]";
      break;
    case ResultCode::kErrorEnrollKeysApiCallBadRequest:
      stream << "[EnrollKeys API call failed: Bad request]";
      break;
    case ResultCode::kErrorEnrollKeysApiCallResponseMalformed:
      stream << "[EnrollKeys API call failed: Response malformed]";
      break;
    case ResultCode::kErrorEnrollKeysApiCallInternalServerError:
      stream << "[EnrollKeys API call failed: Internal server error]";
      break;
    case ResultCode::kErrorEnrollKeysApiCallUnknownError:
      stream << "[EnrollKeys API call failed: Unknown error]";
      break;
    case ResultCode::kErrorCryptAuthServerOverloaded:
      stream << "[Error: CryptAuth server overloaded]";
      break;
    case ResultCode::kErrorSyncKeysResponseMissingRandomSessionId:
      stream << "[Error: SyncKeysResponse missing session ID]";
      break;
    case ResultCode::kErrorSyncKeysResponseInvalidClientDirective:
      stream << "[Error: Missing/invalid ClientDirective in SyncKeysResponse]";
      break;
    case ResultCode::kErrorWrongNumberOfSyncSingleKeyResponses:
      stream << "[Error: Wrong number of SyncSingleKeyResponses]";
      break;
    case ResultCode::kErrorWrongNumberOfKeyActions:
      stream << "[Error: Wrong number of key actions]";
      break;
    case ResultCode::kErrorInvalidKeyActionEnumValue:
      stream << "[Error: Invalid KeyAction enum value]";
      break;
    case ResultCode::kErrorKeyActionsSpecifyMultipleActiveKeys:
      stream << "[Error: KeyActions specify multiple active keys]";
      break;
    case ResultCode::kErrorKeyActionsDoNotSpecifyAnActiveKey:
      stream << "[Error: KeyActions do not specify an active key]";
      break;
    case ResultCode::kErrorKeyCreationKeyTypeNotSupported:
      stream << "[Error: Key-creation instructions specify unsupported "
             << "KeyType]";
      break;
    case ResultCode::kErrorUserKeyPairCreationInstructionsInvalid:
      stream << "[Error: Key-creation instructions for user key pair invalid]";
      break;
    case ResultCode::kErrorSymmetricKeyCreationMissingServerDiffieHellman:
      stream << "[Error: Cannot create symmetric key; missing server "
             << "Diffie-Hellman key]";
      break;
    case ResultCode::kErrorKeyProofComputationFailed:
      stream << "[Error: Failed to compute valid key proof]";
      break;
    case ResultCode::kErrorTimeoutWaitingForSyncKeysResponse:
      stream << "[Error: Timeout waiting for SyncKeys response]";
      break;
    case ResultCode::kErrorTimeoutWaitingForKeyCreation:
      stream << "[Error: Timeout waiting for key creation]";
      break;
    case ResultCode::kErrorTimeoutWaitingForEnrollKeysResponse:
      stream << "[Error: Timeout waiting for EnrollKeys response]";
      break;
    case ResultCode::kErrorGcmRegistrationFailed:
      stream << "[Error: GCM registration failed]";
      break;
    case ResultCode::kErrorClientAppMetadataFetchFailed:
      stream << "[Error: Could not retrieve ClientAppMetadata from "
             << "ClientAppMetadataProvider]";
      break;
    case ResultCode::kErrorTimeoutWaitingForGcmRegistration:
      stream << "[Error: Timeout waiting for GCM registration]";
      break;
    case ResultCode::kErrorTimeoutWaitingForClientAppMetadata:
      stream << "[Error: Timeout waiting for ClientAppMetadata]";
      break;
    case ResultCode::kErrorUserKeyPairCreationFailed:
      stream << "[Error: Failed to create user key pair]";
      break;
    case ResultCode::kErrorLegacyAuthzenKeyCreationFailed:
      stream << "[Error: Failed to create legacy authzen key]";
      break;
    case ResultCode::kErrorDeviceSyncBetterTogetherKeyCreationFailed:
      stream << "[Error: Failed to create DeviceSync:BetterTogether key pair]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace ash
