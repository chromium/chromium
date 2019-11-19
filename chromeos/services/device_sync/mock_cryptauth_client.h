// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_MOCK_CRYPTAUTH_CLIENT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_MOCK_CRYPTAUTH_CLIENT_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/services/device_sync/cryptauth_client.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_enrollment.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

namespace device_sync {

class MockCryptAuthClient : public CryptAuthClient {
 public:
  MockCryptAuthClient();
  ~MockCryptAuthClient() override;

  // TODO(https://crbug.com/997268): Update these to use MOCK_METHOD.
  // CryptAuthClient:
  MOCK_METHOD4(GetMyDevices,
               void(const cryptauth::GetMyDevicesRequest& request,
                    const GetMyDevicesCallback& callback,
                    const ErrorCallback& error_callback,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation));
  MOCK_METHOD3(FindEligibleUnlockDevices,
               void(const cryptauth::FindEligibleUnlockDevicesRequest& request,
                    const FindEligibleUnlockDevicesCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(FindEligibleForPromotion,
               void(const cryptauth::FindEligibleForPromotionRequest& request,
                    const FindEligibleForPromotionCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD4(SendDeviceSyncTickle,
               void(const cryptauth::SendDeviceSyncTickleRequest& request,
                    const SendDeviceSyncTickleCallback& callback,
                    const ErrorCallback& error_callback,
                    const net::PartialNetworkTrafficAnnotationTag&
                        partial_traffic_annotation));
  MOCK_METHOD3(ToggleEasyUnlock,
               void(const cryptauth::ToggleEasyUnlockRequest& request,
                    const ToggleEasyUnlockCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(SetupEnrollment,
               void(const cryptauth::SetupEnrollmentRequest& request,
                    const SetupEnrollmentCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(FinishEnrollment,
               void(const cryptauth::FinishEnrollmentRequest& request,
                    const FinishEnrollmentCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(SyncKeys,
               void(const cryptauthv2::SyncKeysRequest& request,
                    const SyncKeysCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(EnrollKeys,
               void(const cryptauthv2::EnrollKeysRequest& request,
                    const EnrollKeysCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(SyncMetadata,
               void(const cryptauthv2::SyncMetadataRequest& request,
                    const SyncMetadataCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(ShareGroupPrivateKey,
               void(const cryptauthv2::ShareGroupPrivateKeyRequest& request,
                    const ShareGroupPrivateKeyCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(BatchNotifyGroupDevices,
               void(const cryptauthv2::BatchNotifyGroupDevicesRequest& request,
                    const BatchNotifyGroupDevicesCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(BatchGetFeatureStatuses,
               void(const cryptauthv2::BatchGetFeatureStatusesRequest& request,
                    const BatchGetFeatureStatusesCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(BatchSetFeatureStatuses,
               void(const cryptauthv2::BatchSetFeatureStatusesRequest& request,
                    const BatchSetFeatureStatusesCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(GetDevicesActivityStatus,
               void(const cryptauthv2::GetDevicesActivityStatusRequest& request,
                    const GetDevicesActivityStatusCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD0(GetAccessTokenUsed, std::string());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCryptAuthClient);
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

  DISALLOW_COPY_AND_ASSIGN(MockCryptAuthClientFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_MOCK_CRYPTAUTH_CLIENT_H_
