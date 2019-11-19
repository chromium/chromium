// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace syncer {

class FakeLocalDeviceInfoProvider : public LocalDeviceInfoProvider {
 public:
  FakeLocalDeviceInfoProvider();
  ~FakeLocalDeviceInfoProvider() override;

  // Overrides for LocalDeviceInfoProvider.
  version_info::Channel GetChannel() const override;
  const DeviceInfo* GetLocalDeviceInfo() const override;
  std::unique_ptr<Subscription> RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override;

  void SetReady(bool ready);
  DeviceInfo* GetMutableDeviceInfo();

 private:
  DeviceInfo device_info_;
  bool ready_ = true;
  base::CallbackList<void(void)> callback_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeLocalDeviceInfoProvider);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_FAKE_LOCAL_DEVICE_INFO_PROVIDER_H_
