// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNC_RESULT_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNC_RESULT_H_

#include <optional>
#include <ostream>

#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

namespace ash {

namespace device_sync {

// Information about the result of a CryptAuth v2 DeviceSync attempt.
class CryptAuthDeviceSyncResult {
 public:
  // Enum class to denote the result of a CryptAuth v2 DeviceSync attempt.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class ResultCode {
    kSuccess = 0,
    kFinishedWithNonFatalErrors = 1,
    kErrorClientAppMetadataFetchFailed = 2,
    kErrorMissingUserKeyPair = 3,
    kErrorCreatingGroupKey = 4,
    kErrorEncryptingDeviceMetadata = 5,
    kErrorEstablishingGroupPublicKey = 6,
    kErrorNoMetadataInResponse = 7,
    kErrorAllResponseMetadataInvalid = 8,
    kErrorNoLocalDeviceMetadataInResponse = 9,
    kErrorMissingLocalDeviceFeatureStatuses = 10,
    kErrorMissingLocalDeviceSyncBetterTogetherKey = 11,
    kErrorDecryptingGroupPrivateKey = 12,
    kErrorEncryptingGroupPrivateKey = 13,
    kErrorSyncMetadataApiCallOffline = 14,
    kErrorSyncMetadataApiCallEndpointNotFound = 15,
    kErrorSyncMetadataApiCallAuthenticationError = 16,
    kErrorSyncMetadataApiCallBadRequest = 17,
    kErrorSyncMetadataApiCallResponseMalformed = 18,
    kErrorSyncMetadataApiCallInternalServerError = 19,
    kErrorSyncMetadataApiCallUnknownError = 20,
    kErrorBatchGetFeatureStatusesApiCallOffline = 21,
    kErrorBatchGetFeatureStatusesApiCallEndpointNotFound = 22,
    kErrorBatchGetFeatureStatusesApiCallAuthenticationError = 23,
    kErrorBatchGetFeatureStatusesApiCallBadRequest = 24,
    kErrorBatchGetFeatureStatusesApiCallResponseMalformed = 25,
    kErrorBatchGetFeatureStatusesApiCallInternalServerError = 26,
    kErrorBatchGetFeatureStatusesApiCallUnknownError = 27,
    kErrorShareGroupPrivateKeyApiCallOffline = 28,
    kErrorShareGroupPrivateKeyApiCallEndpointNotFound = 29,
    kErrorShareGroupPrivateKeyApiCallAuthenticationError = 30,
    kErrorShareGroupPrivateKeyApiCallBadRequest = 31,
    kErrorShareGroupPrivateKeyApiCallResponseMalformed = 32,
    kErrorShareGroupPrivateKeyApiCallInternalServerError = 33,
    kErrorShareGroupPrivateKeyApiCallUnknownError = 34,
    kErrorTimeoutWaitingForGroupKeyCreation = 35,
    kErrorTimeoutWaitingForClientAppMetadata = 36,
    kErrorTimeoutWaitingForLocalDeviceMetadataEncryption = 37,
    kErrorTimeoutWaitingForFirstSyncMetadataResponse = 38,
    kErrorTimeoutWaitingForSecondSyncMetadataResponse = 39,
    kErrorTimeoutWaitingForGroupPrivateKeyDecryption = 40,
    kErrorTimeoutWaitingForDeviceMetadataDecryption = 41,
    kErrorTimeoutWaitingForBatchGetFeatureStatusesResponse = 42,
    kErrorTimeoutWaitingForGroupPrivateKeyEncryption = 43,
    kErrorTimeoutWaitingForShareGroupPrivateKeyResponse = 44,
    // Used for UMA logs.
    kMaxValue = kErrorTimeoutWaitingForShareGroupPrivateKeyResponse
  };

  // Enum class to denote the result type of a CryptAuth v2 DeviceSync attempt.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class ResultType {
    kSuccess = 0,
    kNonFatalError = 1,
    kFatalError = 2,
    // Used for UMA logs.
    kMaxValue = kFatalError
  };

  static ResultType GetResultType(ResultCode result_code);

  CryptAuthDeviceSyncResult(
      ResultCode result_code,
      bool did_device_registry_change,
      const std::optional<cryptauthv2::ClientDirective>& client_directive);
  CryptAuthDeviceSyncResult(const CryptAuthDeviceSyncResult& other);

  ~CryptAuthDeviceSyncResult();

  ResultCode result_code() const { return result_code_; }

  const std::optional<cryptauthv2::ClientDirective>& client_directive() const {
    return client_directive_;
  }

  bool did_device_registry_change() const {
    return did_device_registry_change_;
  }

  ResultType GetResultType() const;
  bool IsSuccess() const;

  bool operator==(const CryptAuthDeviceSyncResult& other) const;
  bool operator!=(const CryptAuthDeviceSyncResult& other) const;

 private:
  ResultCode result_code_;
  bool did_device_registry_change_;
  std::optional<cryptauthv2::ClientDirective> client_directive_;
};

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthDeviceSyncResult::ResultCode result_code);

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNC_RESULT_H_
