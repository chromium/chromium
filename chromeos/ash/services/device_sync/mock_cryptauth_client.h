// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_MOCK_CRYPTAUTH_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_MOCK_CRYPTAUTH_CLIENT_H_

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "chromeos/ash/services/device_sync/cryptauth_client.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_enrollment.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace device_sync {

class MockCryptAuthClient : public CryptAuthClient {
 public:
  MockCryptAuthClient();

  MockCryptAuthClient(const MockCryptAuthClient&) = delete;
  MockCryptAuthClient& operator=(const MockCryptAuthClient&) = delete;

  ~MockCryptAuthClient() override;

  // TODO(crbug.com/41477954): Update these to use MOCK_METHOD.
  // CryptAuthClient:
  void GetMyDevices(const cryptauth::GetMyDevicesRequest& request,
                    GetMyDevicesCallback callback,
                    ErrorCallback error_callback,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation) override {
    GetMyDevices_(request, callback, error_callback,
                  partial_traffic_annotation);
  }
  MOCK_METHOD4(GetMyDevices_,
               void(const cryptauth::GetMyDevicesRequest& request,
                    GetMyDevicesCallback& callback,
                    ErrorCallback& error_callback,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation));
  MOCK_METHOD3(FindEligibleUnlockDevices,
               void(const cryptauth::FindEligibleUnlockDevicesRequest& request,
                    FindEligibleUnlockDevicesCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(FindEligibleForPromotion,
               void(const cryptauth::FindEligibleForPromotionRequest& request,
                    FindEligibleForPromotionCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD4(SendDeviceSyncTickle,
               void(const cryptauth::SendDeviceSyncTickleRequest& request,
                    SendDeviceSyncTickleCallback callback,
                    ErrorCallback error_callback,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation));
  MOCK_METHOD3(ToggleEasyUnlock,
               void(const cryptauth::ToggleEasyUnlockRequest& request,
                    ToggleEasyUnlockCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(SetupEnrollment,
               void(const cryptauth::SetupEnrollmentRequest& request,
                    SetupEnrollmentCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(FinishEnrollment,
               void(const cryptauth::FinishEnrollmentRequest& request,
                    FinishEnrollmentCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(SyncKeys,
               void(const cryptauthv2::SyncKeysRequest& request,
                    SyncKeysCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(EnrollKeys,
               void(const cryptauthv2::EnrollKeysRequest& request,
                    EnrollKeysCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(SyncMetadata,
               void(const cryptauthv2::SyncMetadataRequest& request,
                    SyncMetadataCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(ShareGroupPrivateKey,
               void(const cryptauthv2::ShareGroupPrivateKeyRequest& request,
                    ShareGroupPrivateKeyCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(BatchNotifyGroupDevices,
               void(const cryptauthv2::BatchNotifyGroupDevicesRequest& request,
                    BatchNotifyGroupDevicesCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(BatchGetFeatureStatuses,
               void(const cryptauthv2::BatchGetFeatureStatusesRequest& request,
                    BatchGetFeatureStatusesCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(BatchSetFeatureStatuses,
               void(const cryptauthv2::BatchSetFeatureStatusesRequest& request,
                    BatchSetFeatureStatusesCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD3(GetDevicesActivityStatus,
               void(const cryptauthv2::GetDevicesActivityStatusRequest& request,
                    GetDevicesActivityStatusCallback callback,
                    ErrorCallback error_callback));
  MOCK_METHOD0(GetAccessTokenUsed, std::string());
};

class MockCryptAuthClientFactory : public CryptAuthClientFactory {
 public:
  class Observer {
   public:
    // Called with the new instance when it is requested from the factory,
    // allowing expectations to be set. Ownership of |client| will be taken by
    // the caller of CreateInstance().
    virtual void OnCryptAuthClientCreated(MockCryptAuthClient* client) = 0;
  };

  // Represents the type of mock instances to create.
  enum class MockType { MAKE_NICE_MOCKS, MAKE_STRICT_MOCKS };

  // If |mock_type| is STRICT, then StrictMocks will be created. Otherwise,
  // NiceMocks will be created.
  explicit MockCryptAuthClientFactory(MockType mock_type);

  MockCryptAuthClientFactory(const MockCryptAuthClientFactory&) = delete;
  MockCryptAuthClientFactory& operator=(const MockCryptAuthClientFactory&) =
      delete;

  ~MockCryptAuthClientFactory() override;

  // CryptAuthClientFactory:
  std::unique_ptr<CryptAuthClient> CreateInstance() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Whether to create StrictMocks or NiceMocks.
  const MockType mock_type_;

  // Observers of the factory.
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_MOCK_CRYPTAUTH_CLIENT_H_
