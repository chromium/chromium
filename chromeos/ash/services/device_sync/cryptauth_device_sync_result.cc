// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"

namespace ash {

namespace device_sync {

// static
CryptAuthDeviceSyncResult::ResultType CryptAuthDeviceSyncResult::GetResultType(
    ResultCode result_code) {
  switch (result_code) {
    case CryptAuthDeviceSyncResult::ResultCode::kSuccess:
      return CryptAuthDeviceSyncResult::ResultType::kSuccess;
    case CryptAuthDeviceSyncResult::ResultCode::kFinishedWithNonFatalErrors:
      return CryptAuthDeviceSyncResult::ResultType::kNonFatalError;
    default:
      return CryptAuthDeviceSyncResult::ResultType::kFatalError;
  }
}

CryptAuthDeviceSyncResult::CryptAuthDeviceSyncResult(
    ResultCode result_code,
    bool did_device_registry_change,
    const std::optional<cryptauthv2::ClientDirective>& client_directive)
    : result_code_(result_code),
      did_device_registry_change_(did_device_registry_change),
      client_directive_(client_directive) {}

CryptAuthDeviceSyncResult::CryptAuthDeviceSyncResult(
    const CryptAuthDeviceSyncResult& other) = default;

CryptAuthDeviceSyncResult::~CryptAuthDeviceSyncResult() = default;

CryptAuthDeviceSyncResult::ResultType CryptAuthDeviceSyncResult::GetResultType()
    const {
  return GetResultType(result_code_);
}

bool CryptAuthDeviceSyncResult::IsSuccess() const {
  return GetResultType(result_code_) == ResultType::kSuccess;
}

bool CryptAuthDeviceSyncResult::operator==(
    const CryptAuthDeviceSyncResult& other) const {
  bool client_directives_agree =
      (!client_directive_.has_value() &&
       !other.client_directive_.has_value()) ||
      (client_directive_.has_value() && other.client_directive_.has_value() &&
       client_directive_->SerializeAsString() ==
           other.client_directive_->SerializeAsString());
  return client_directives_agree && result_code_ == other.result_code_ &&
         did_device_registry_change_ == other.did_device_registry_change_;
}

bool CryptAuthDeviceSyncResult::operator!=(
    const CryptAuthDeviceSyncResult& other) const {
  return !(*this == other);
}

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthDeviceSyncResult::ResultCode result_code) {
  using ResultCode = CryptAuthDeviceSyncResult::ResultCode;

  switch (result_code) {
    case ResultCode::kSuccess:
      stream << "[Success]";
      break;
    case ResultCode::kFinishedWithNonFatalErrors:
      stream << "[Finished with non-fatal errors]";
      break;
    case ResultCode::kErrorClientAppMetadataFetchFailed:
      stream << "[Error: Could not retrieve ClientAppMetadata from "
             << "ClientAppMetadataProvider]";
      break;
    case ResultCode::kErrorMissingUserKeyPair:
      stream << "[Error: No user key pair in registry]";
      break;
    case ResultCode::kErrorCreatingGroupKey:
      stream << "[Error: Could not create group key]";
      break;
    case ResultCode::kErrorEncryptingDeviceMetadata:
      stream << "[Error: Could not encrypt local device metadata]";
      break;
    case ResultCode::kErrorEstablishingGroupPublicKey:
      stream << "[Error: Could not establish group public key]";
      break;
    case ResultCode::kErrorNoMetadataInResponse:
      stream << "[Error: No encrypted metadata in SyncMetadata response]";
      break;
    case ResultCode::kErrorAllResponseMetadataInvalid:
      stream << "[Error: All DeviceMetadataPackets in SyncMetadata "
             << "response are invalid]";
      break;
    case ResultCode::kErrorNoLocalDeviceMetadataInResponse:
      stream << "[Error: No local device metadata in SyncMetadata response]";
      break;
    case ResultCode::kErrorMissingLocalDeviceFeatureStatuses:
      stream << "[Error: No local device feature statuses]";
      break;
    case ResultCode::kErrorMissingLocalDeviceSyncBetterTogetherKey:
      stream << "[Error: No DeviceSync:BetterTogether key in registry]";
      break;
    case ResultCode::kErrorDecryptingGroupPrivateKey:
      stream << "[Error: Could not decrypt group private key]";
      break;
    case ResultCode::kErrorEncryptingGroupPrivateKey:
      stream << "[Error: Could not encrypt group private key]";
      break;
    case ResultCode::kErrorSyncMetadataApiCallOffline:
      stream << "[SyncMetadata API call failed: Offline]";
      break;
    case ResultCode::kErrorSyncMetadataApiCallEndpointNotFound:
      stream << "[SyncMetadata API call failed: Endpoint not found]";
      break;
    case ResultCode::kErrorSyncMetadataApiCallAuthenticationError:
      stream << "[SyncMetadata API call failed: Authentication error]";
      break;
    case ResultCode::kErrorSyncMetadataApiCallBadRequest:
      stream << "[SyncMetadata API call failed: Bad request]";
      break;
    case ResultCode::kErrorSyncMetadataApiCallResponseMalformed:
      stream << "[SyncMetadata API call failed: Response malformed]";
      break;
    case ResultCode::kErrorSyncMetadataApiCallInternalServerError:
      stream << "[SyncMetadata API call failed: Internal server error]";
      break;
    case ResultCode::kErrorSyncMetadataApiCallUnknownError:
      stream << "[SyncMetadata API call failed: Unknown error]";
      break;
    case ResultCode::kErrorBatchGetFeatureStatusesApiCallOffline:
      stream << "[BatchGetFeatureStatuses API call failed: Offline]";
      break;
    case ResultCode::kErrorBatchGetFeatureStatusesApiCallEndpointNotFound:
      stream << "[BatchGetFeatureStatuses API call failed: Endpoint not found]";
      break;
    case ResultCode::kErrorBatchGetFeatureStatusesApiCallAuthenticationError:
      stream << "[BatchGetFeatureStatuses API call failed: Authentication "
             << "error]";
      break;
    case ResultCode::kErrorBatchGetFeatureStatusesApiCallBadRequest:
      stream << "[BatchGetFeatureStatuses API call failed: Bad request]";
      break;
    case ResultCode::kErrorBatchGetFeatureStatusesApiCallResponseMalformed:
      stream << "[BatchGetFeatureStatuses API call failed: Response malformed]";
      break;
    case ResultCode::kErrorBatchGetFeatureStatusesApiCallInternalServerError:
      stream << "[BatchGetFeatureStatuses API call failed: Internal server "
             << "error]";
      break;
    case ResultCode::kErrorBatchGetFeatureStatusesApiCallUnknownError:
      stream << "[BatchGetFeatureStatuses API call failed: Unknown error]";
      break;
    case ResultCode::kErrorShareGroupPrivateKeyApiCallOffline:
      stream << "[ShareGroupPrivateKey API call failed: Offline]";
      break;
    case ResultCode::kErrorShareGroupPrivateKeyApiCallEndpointNotFound:
      stream << "[ShareGroupPrivateKey API call failed: Endpoint not found]";
      break;
    case ResultCode::kErrorShareGroupPrivateKeyApiCallAuthenticationError:
      stream << "[ShareGroupPrivateKey API call failed: Authentication error]";
      break;
    case ResultCode::kErrorShareGroupPrivateKeyApiCallBadRequest:
      stream << "[ShareGroupPrivateKey API call failed: Bad request]";
      break;
    case ResultCode::kErrorShareGroupPrivateKeyApiCallResponseMalformed:
      stream << "[ShareGroupPrivateKey API call failed: Response malformed]";
      break;
    case ResultCode::kErrorShareGroupPrivateKeyApiCallInternalServerError:
      stream << "[ShareGroupPrivateKey API call failed: Internal server error]";
      break;
    case ResultCode::kErrorShareGroupPrivateKeyApiCallUnknownError:
      stream << "[ShareGroupPrivateKey API call failed: Unknown error]";
      break;
    case ResultCode::kErrorTimeoutWaitingForClientAppMetadata:
      stream << "[Error: Timeout waiting for ClientAppMetadata]";
      break;
    case ResultCode::kErrorTimeoutWaitingForGroupKeyCreation:
      stream << "[Error: Timeout waiting for group key creation]";
      break;
    case ResultCode::kErrorTimeoutWaitingForLocalDeviceMetadataEncryption:
      stream << "[Error: Timeout waiting for local device metadata encryption]";
      break;
    case ResultCode::kErrorTimeoutWaitingForFirstSyncMetadataResponse:
      stream << "[Error: Timeout waiting for first SyncMetadata response]";
      break;
    case ResultCode::kErrorTimeoutWaitingForSecondSyncMetadataResponse:
      stream << "[Error: Timeout waiting for second SyncMetadata response]";
      break;
    case ResultCode::kErrorTimeoutWaitingForGroupPrivateKeyDecryption:
      stream << "[Error: Timeout waiting for group private key decryption]";
      break;
    case ResultCode::kErrorTimeoutWaitingForDeviceMetadataDecryption:
      stream << "[Error: Timeout waiting for remote device metadata "
             << "decryption]";
      break;
    case ResultCode::kErrorTimeoutWaitingForBatchGetFeatureStatusesResponse:
      stream << "[Error: Timeout waiting for BatchGetFeatureStatuses response]";
      break;
    case ResultCode::kErrorTimeoutWaitingForGroupPrivateKeyEncryption:
      stream << "[Error: Timeout waiting for group private key encryption]";
      break;
    case ResultCode::kErrorTimeoutWaitingForShareGroupPrivateKeyResponse:
      stream << "[Error: Timeout waiting for ShareGroupPrivateKey response]";
      break;
  }

  return stream;
}

}  // namespace device_sync

}  // namespace ash
