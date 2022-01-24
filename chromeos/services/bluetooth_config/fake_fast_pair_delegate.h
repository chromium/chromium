// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_

#include "chromeos/services/bluetooth_config/fast_pair_delegate.h"

namespace chromeos {
namespace bluetooth_config {

class DeviceNameManager;

class FakeFastPairDelegate : public FastPairDelegate {
 public:
  FakeFastPairDelegate();
  FakeFastPairDelegate(const FakeFastPairDelegate&) = delete;
  FakeFastPairDelegate& operator=(const FakeFastPairDelegate&) = delete;
  ~FakeFastPairDelegate() override;

  DeviceNameManager* device_name_manager() { return device_name_manager_; }

  // FastPairDelegate:
  void SetDeviceNameManager(DeviceNameManager* device_name_manager) override;

 private:
  DeviceNameManager* device_name_manager_ = nullptr;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_MOCK_FAST_PAIR_DELEGATE_H_
