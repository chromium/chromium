// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace syncer {

class DeviceInfoSyncClient;

class LocalDeviceInfoProviderImpl : public MutableLocalDeviceInfoProvider {
 public:
  LocalDeviceInfoProviderImpl(version_info::Channel channel,
                              const std::string& version,
                              const DeviceInfoSyncClient* sync_client);

  LocalDeviceInfoProviderImpl(const LocalDeviceInfoProviderImpl&) = delete;
  LocalDeviceInfoProviderImpl& operator=(const LocalDeviceInfoProviderImpl&) =
      delete;

  ~LocalDeviceInfoProviderImpl() override;

  // MutableLocalDeviceInfoProvider implementation.
  void Initialize(const std::string& cache_guid,
                  const std::string& client_name,
                  const std::string& manufacturer_name,
                  const std::string& model_name,
                  const std::string& full_hardware_class,
                  const DeviceInfo* device_info_restored_from_store) override;
  void Clear() override;
  void UpdateClientName(const std::string& client_name) override;
  void UpdateRecentSignInTime(base::Time time) override;
  version_info::Channel GetChannel() const override;
  const DeviceInfo* GetLocalDeviceInfo() const override;
  base::CallbackListSubscription RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override;

 private:
  // The channel (CANARY, DEV, BETA, etc.) of the current client.
  const version_info::Channel channel_;

  // The version string for the current client.
  const std::string version_;

  void ResetFullHardwareClassIfUmaDisabled() const;

  const raw_ptr<const DeviceInfoSyncClient> sync_client_;

  bool IsUmaEnabledOnCrOSDevice() const;

  // The |full_hardware_class| is stored in order to handle UMA toggles
  // during a users session. Tracking |full_hardware_class| in this class
  // ensures it's reset/retrieved correctly when GetLocalDeviceInfo() is called.
  std::string full_hardware_class_;
  std::unique_ptr<DeviceInfo> local_device_info_;
  base::RepeatingClosureList closure_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LocalDeviceInfoProviderImpl> weak_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
