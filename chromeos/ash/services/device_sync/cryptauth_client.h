// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace cryptauth {
class GetMyDevicesRequest;
class GetMyDevicesResponse;
class FindEligibleUnlockDevicesRequest;
class FindEligibleUnlockDevicesResponse;
class SendDeviceSyncTickleRequest;
class SendDeviceSyncTickleResponse;
class ToggleEasyUnlockRequest;
class ToggleEasyUnlockResponse;
class SetupEnrollmentRequest;
class SetupEnrollmentResponse;
class FinishEnrollmentRequest;
class FinishEnrollmentResponse;
class FindEligibleForPromotionRequest;
class FindEligibleForPromotionResponse;
}  // namespace cryptauth

namespace cryptauthv2 {
class SyncKeysRequest;
class SyncKeysResponse;
class EnrollKeysRequest;
class EnrollKeysResponse;
class SyncMetadataRequest;
class SyncMetadataResponse;
class ShareGroupPrivateKeyRequest;
class ShareGroupPrivateKeyResponse;
class BatchNotifyGroupDevicesRequest;
class BatchNotifyGroupDevicesResponse;
class BatchGetFeatureStatusesRequest;
class BatchGetFeatureStatusesResponse;
class BatchSetFeatureStatusesRequest;
class BatchSetFeatureStatusesResponse;
class GetDevicesActivityStatusRequest;
class GetDevicesActivityStatusResponse;
}  // namespace cryptauthv2

namespace ash {

namespace device_sync {

// Interface for making API requests to the CryptAuth service, which
// manages cryptographic credentials (ie. public keys) for a user's devices.
// Implmentations shall only processes a single request, so create a new
// instance for each request you make. DO NOT REUSE.
// For documentation on each API call, see
// chromeos/ash/services/device_sync/proto/cryptauth_api.proto,
// chromeos/ash/services/device_sync/proto/cryptauth_enrollment.proto, and
// chromeos/ash/services/device_sync/proto/cryptauth_devicesync.proto.
class CryptAuthClient {
 public:
  typedef base::OnceCallback<void(NetworkRequestError)> ErrorCallback;

  virtual ~CryptAuthClient() {}

  // DeviceSync v1: GetMyDevices
  typedef base::OnceCallback<void(const cryptauth::GetMyDevicesResponse&)>
      GetMyDevicesCallback;
  virtual void GetMyDevices(const cryptauth::GetMyDevicesRequest& request,
                            GetMyDevicesCallback callback,
                            ErrorCallback error_callback,
                            const net::PartialNetworkTrafficAnnotationTag&
                                partial_traffic_annotation) = 0;

  // DeviceSync v1: FindEligibleUnlockDevices
  typedef base::OnceCallback<void(
      const cryptauth::FindEligibleUnlockDevicesResponse&)>
      FindEligibleUnlockDevicesCallback;
  virtual void FindEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesRequest& request,
      FindEligibleUnlockDevicesCallback callback,
      ErrorCallback error_callback) = 0;

  // DeviceSync v1: FindEligibleForPromotion
  typedef base::OnceCallback<void(
      const cryptauth::FindEligibleForPromotionResponse&)>
      FindEligibleForPromotionCallback;
  virtual void FindEligibleForPromotion(
      const cryptauth::FindEligibleForPromotionRequest& request,
      FindEligibleForPromotionCallback callback,
      ErrorCallback error_callback) = 0;

  // DeviceSync v1: SendDeviceSyncTickle
  typedef base::OnceCallback<void(
      const cryptauth::SendDeviceSyncTickleResponse&)>
      SendDeviceSyncTickleCallback;
  virtual void SendDeviceSyncTickle(
      const cryptauth::SendDeviceSyncTickleRequest& request,
      SendDeviceSyncTickleCallback callback,
      ErrorCallback error_callback,
      const net::PartialNetworkTrafficAnnotationTag&
          partial_traffic_annotation) = 0;

  // DeviceSync v1: ToggleEasyUnlock
  typedef base::OnceCallback<void(const cryptauth::ToggleEasyUnlockResponse&)>
      ToggleEasyUnlockCallback;
  virtual void ToggleEasyUnlock(
      const cryptauth::ToggleEasyUnlockRequest& request,
      ToggleEasyUnlockCallback callback,
      ErrorCallback error_callback) = 0;

  // Enrollment v1: SetupEnrollment
  typedef base::OnceCallback<void(const cryptauth::SetupEnrollmentResponse&)>
      SetupEnrollmentCallback;
  virtual void SetupEnrollment(const cryptauth::SetupEnrollmentRequest& request,
                               SetupEnrollmentCallback callback,
                               ErrorCallback error_callback) = 0;

  // Enrollment v1: FinishEnrollment
  typedef base::OnceCallback<void(const cryptauth::FinishEnrollmentResponse&)>
      FinishEnrollmentCallback;
  virtual void FinishEnrollment(
      const cryptauth::FinishEnrollmentRequest& request,
      FinishEnrollmentCallback callback,
      ErrorCallback error_callback) = 0;

  // Enrollment v2: SyncKeys
  typedef base::OnceCallback<void(const cryptauthv2::SyncKeysResponse&)>
      SyncKeysCallback;
  virtual void SyncKeys(const cryptauthv2::SyncKeysRequest& request,
                        SyncKeysCallback callback,
                        ErrorCallback error_callback) = 0;

  // Enrollment v2: EnrollKeys
  typedef base::OnceCallback<void(const cryptauthv2::EnrollKeysResponse&)>
      EnrollKeysCallback;
  virtual void EnrollKeys(const cryptauthv2::EnrollKeysRequest& request,
                          EnrollKeysCallback callback,
                          ErrorCallback error_callback) = 0;

  // DeviceSync v2: SyncMetadata
  typedef base::OnceCallback<void(const cryptauthv2::SyncMetadataResponse&)>
      SyncMetadataCallback;
  virtual void SyncMetadata(const cryptauthv2::SyncMetadataRequest& request,
                            SyncMetadataCallback callback,
                            ErrorCallback error_callback) = 0;

  // DeviceSync v2: ShareGroupPrivateKey
  typedef base::OnceCallback<void(
      const cryptauthv2::ShareGroupPrivateKeyResponse&)>
      ShareGroupPrivateKeyCallback;
  virtual void ShareGroupPrivateKey(
      const cryptauthv2::ShareGroupPrivateKeyRequest& request,
      ShareGroupPrivateKeyCallback callback,
      ErrorCallback error_callback) = 0;

  // DeviceSync v2: BatchNotifyGroupDevices
  typedef base::OnceCallback<void(
      const cryptauthv2::BatchNotifyGroupDevicesResponse&)>
      BatchNotifyGroupDevicesCallback;
  virtual void BatchNotifyGroupDevices(
      const cryptauthv2::BatchNotifyGroupDevicesRequest& request,
      BatchNotifyGroupDevicesCallback callback,
      ErrorCallback error_callback) = 0;

  // DeviceSync v2: BatchGetFeatureStatuses
  typedef base::OnceCallback<void(
      const cryptauthv2::BatchGetFeatureStatusesResponse&)>
      BatchGetFeatureStatusesCallback;
  virtual void BatchGetFeatureStatuses(
      const cryptauthv2::BatchGetFeatureStatusesRequest& request,
      BatchGetFeatureStatusesCallback callback,
      ErrorCallback error_callback) = 0;

  // DeviceSync v2: BatchSetFeatureStatuses
  typedef base::OnceCallback<void(
      const cryptauthv2::BatchSetFeatureStatusesResponse&)>
      BatchSetFeatureStatusesCallback;
  virtual void BatchSetFeatureStatuses(
      const cryptauthv2::BatchSetFeatureStatusesRequest& request,
      BatchSetFeatureStatusesCallback callback,
      ErrorCallback error_callback) = 0;

  // DeviceSync v2: GetDevicesActivityStatus
  typedef base::OnceCallback<void(
      const cryptauthv2::GetDevicesActivityStatusResponse&)>
      GetDevicesActivityStatusCallback;
  virtual void GetDevicesActivityStatus(
      const cryptauthv2::GetDevicesActivityStatusRequest& request,
      GetDevicesActivityStatusCallback callback,
      ErrorCallback error_callback) = 0;

  // Returns the access token used to make the request. If no request has been
  // made yet, this function will return an empty string.
  virtual std::string GetAccessTokenUsed() = 0;
};

// Interface for creating CryptAuthClient instances. Because each
// CryptAuthClient instance can only be used for one API call, a factory makes
// it easier to make multiple requests in sequence or in parallel.
class CryptAuthClientFactory {
 public:
  virtual ~CryptAuthClientFactory() {}

  virtual std::unique_ptr<CryptAuthClient> CreateInstance() = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_CLIENT_H_
