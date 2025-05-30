// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_

#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace syncer {

class FakeLocalDeviceInfoProvider : public MutableLocalDeviceInfoProvider {
 public:
  FakeLocalDeviceInfoProvider();

  FakeLocalDeviceInfoProvider(const FakeLocalDeviceInfoProvider&) = delete;
  FakeLocalDeviceInfoProvider& operator=(const FakeLocalDeviceInfoProvider&) =
      delete;

  ~FakeLocalDeviceInfoProvider() override;

  // Overrides for LocalDeviceInfoProvider.
  version_info::Channel GetChannel() const override;
  const DeviceInfo* GetLocalDeviceInfo() const override;
  base::CallbackListSubscription RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override;

  // Overrides for MutableLocalDeviceInfoProvider:
  void Initialize(const std::string& cache_guid,
                  const std::string& client_name,
                  const std::string& manufacturer_name,
                  const std::string& model_name,
                  const std::string& full_hardware_class,
                  const DeviceInfo* device_info_restored_from_store) override;
  void Clear() override;
  void UpdateClientName(const std::string& client_name) override;
  void UpdateRecentSignInTime(base::Time time) override;

  void SetReady(bool ready);
  DeviceInfo* GetMutableDeviceInfo();

 private:
  DeviceInfo device_info_;
  bool ready_ = true;
  base::RepeatingClosureList closure_list_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_
