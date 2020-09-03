// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/invalidations/fcm_registration_token_observer.h"
#include "components/sync/invalidations/interested_data_types_observer.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/version_info/version_info.h"

namespace syncer {

class DeviceInfoSyncClient;
class SyncInvalidationsService;

class LocalDeviceInfoProviderImpl : public MutableLocalDeviceInfoProvider,
                                    public syncer::FCMRegistrationTokenObserver,
                                    public InterestedDataTypesObserver {
 public:
  // |sync_invalidations_service| is used to get an FCM registration token and
  // interested data types. It may be nullptr if sync invalidations are
  // disabled.
  LocalDeviceInfoProviderImpl(
      version_info::Channel channel,
      const std::string& version,
      const DeviceInfoSyncClient* sync_client,
      SyncInvalidationsService* sync_invalidations_service);
  ~LocalDeviceInfoProviderImpl() override;

  // MutableLocalDeviceInfoProvider implementation.
  void Initialize(const std::string& cache_guid,
                  const std::string& client_name,
                  const std::string& manufacturer_name,
                  const std::string& model_name) override;
  void Clear() override;
  void UpdateClientName(const std::string& client_name) override;
  version_info::Channel GetChannel() const override;
  const DeviceInfo* GetLocalDeviceInfo() const override;
  std::unique_ptr<Subscription> RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override;

  // syncer::FCMRegistrationTokenObserver implementation.
  void OnFCMRegistrationTokenChanged() override;

  // InterestedDataTypesObserver implementation.
  void OnInterestedDataTypesChanged() override;

 private:
  std::string GetFCMRegistrationToken() const;

  ModelTypeSet GetInterestedDataTypes() const;

  // The channel (CANARY, DEV, BETA, etc.) of the current client.
  const version_info::Channel channel_;

  // The version string for the current client.
  const std::string version_;

  const DeviceInfoSyncClient* const sync_client_;
  SyncInvalidationsService* sync_invalidations_service_ = nullptr;

  std::unique_ptr<DeviceInfo> local_device_info_;
  base::CallbackList<void(void)> callback_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LocalDeviceInfoProviderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalDeviceInfoProviderImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
