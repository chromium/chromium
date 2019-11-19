// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNC_RESULT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNC_RESULT_H_

#include <ostream>

#include "base/optional.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

namespace chromeos {

namespace device_sync {

// Information about the result of a CryptAuth v2 DeviceSync attempt.
class CryptAuthDeviceSyncResult {
 public:
  // Enum class to denote the result of a CryptAuth v2 DeviceSync attempt.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  // TODO(nohle): Add numeric values.
  enum class ResultCode {
    kSuccess,
    kFinishedWithNonFatalErrors,
    kErrorClientAppMetadataFetchFailed,
    kErrorMissingUserKeyPair,
    kErrorEncryptingDeviceMetadata,
    kErrorEstablishingGroupPublicKey,
    kErrorNoMetadataInResponse,
    kErrorAllResponseMetadataInvalid,
    kErrorNoLocalDeviceMetadataInResponse,
    kErrorMissingLocalDeviceFeatureStatuses,
    kErrorMissingLocalDeviceSyncBetterTogetherKey,
    kErrorDecryptingGroupPrivateKey,
    kErrorEncryptingGroupPrivateKey,
    kErrorSyncMetadataApiCallOffline,
    kErrorSyncMetadataApiCallEndpointNotFound,
    kErrorSyncMetadataApiCallAuthenticationError,
    kErrorSyncMetadataApiCallBadRequest,
    kErrorSyncMetadataApiCallResponseMalformed,
    kErrorSyncMetadataApiCallInternalServerError,
    kErrorSyncMetadataApiCallUnknownError,
    kErrorBatchGetFeatureStatusesApiCallOffline,
    kErrorBatchGetFeatureStatusesApiCallEndpointNotFound,
    kErrorBatchGetFeatureStatusesApiCallAuthenticationError,
    kErrorBatchGetFeatureStatusesApiCallBadRequest,
    kErrorBatchGetFeatureStatusesApiCallResponseMalformed,
    kErrorBatchGetFeatureStatusesApiCallInternalServerError,
    kErrorBatchGetFeatureStatusesApiCallUnknownError,
    kErrorShareGroupPrivateKeyApiCallOffline,
    kErrorShareGroupPrivateKeyApiCallEndpointNotFound,
    kErrorShareGroupPrivateKeyApiCallAuthenticationError,
    kErrorShareGroupPrivateKeyApiCallBadRequest,
    kErrorShareGroupPrivateKeyApiCallResponseMalformed,
    kErrorShareGroupPrivateKeyApiCallInternalServerError,
    kErrorShareGroupPrivateKeyApiCallUnknownError,
    kErrorTimeoutWaitingForGroupKeyCreation,
    kErrorTimeoutWaitingForClientAppMetadata,
    kErrorTimeoutWaitingForLocalDeviceMetadataEncryption,
    kErrorTimeoutWaitingForFirstSyncMetadataResponse,
    kErrorTimeoutWaitingForSecondSyncMetadataResponse,
    kErrorTimeoutWaitingForGroupPrivateKeyDecryption,
    kErrorTimeoutWaitingForDeviceMetadataDecryption,
    kErrorTimeoutWaitingForBatchGetFeatureStatusesResponse,
    kErrorTimeoutWaitingForGroupPrivateKeyEncryption,
    kErrorTimeoutWaitingForShareGroupPrivateKeyResponse,
    // Used for UMA logs.
    kMaxValue = kErrorTimeoutWaitingForShareGroupPrivateKeyResponse
  };

  // Enum class to denote the result type of a CryptAuth v2 DeviceSync attempt.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  // TODO(nohle): Add numeric values.
  enum class ResultType {
    kSuccess,
    kNonFatalError,
    kFatalError,
    // Used for UMA logs.
    kMaxValue = kFatalError
  };

  static ResultType GetResultType(ResultCode result_code);

  CryptAuthDeviceSyncResult(
      ResultCode result_code,
      bool did_device_registry_change,
      const base::Optional<cryptauthv2::ClientDirective>& client_directive);
  CryptAuthDeviceSyncResult(const CryptAuthDeviceSyncResult& other);

  ~CryptAuthDeviceSyncResult();

  ResultCode result_code() const { return result_code_; }

  const base::Optional<cryptauthv2::ClientDirective>& client_directive() const {
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
  base::Optional<cryptauthv2::ClientDirective> client_directive_;
};

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthDeviceSyncResult::ResultCode result_code);

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNC_RESULT_H_
