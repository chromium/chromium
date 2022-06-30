// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_

#include <memory>
#include <vector>

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"

namespace ash::quick_start {

class FakeTargetDeviceConnectionBroker : public TargetDeviceConnectionBroker {
 public:
  class Factory : public TargetDeviceConnectionBrokerFactory {
   public:
    Factory();
    Factory(Factory&) = delete;
    Factory& operator=(Factory&) = delete;
    ~Factory() override;

    // Returns all FakeTargetDeviceConnectionBroker instances created by
    // CreateInstance().
    const std::vector<FakeTargetDeviceConnectionBroker*>& instances() {
      return instances_;
    }

   private:
    std::unique_ptr<TargetDeviceConnectionBroker> CreateInstance() override;

    std::vector<FakeTargetDeviceConnectionBroker*> instances_;
  };

  FakeTargetDeviceConnectionBroker();
  FakeTargetDeviceConnectionBroker(FakeTargetDeviceConnectionBroker&) = delete;
  FakeTargetDeviceConnectionBroker& operator=(
      FakeTargetDeviceConnectionBroker&) = delete;
  ~FakeTargetDeviceConnectionBroker() override;

  // TargetDeviceConnectionBroker:
  FeatureSupportStatus GetFeatureSupportStatus() const override;
  void StartAdvertising(ConnectionLifecycleListener* listener,
                        ResultCallback on_start_advertising_callback) override;
  void StopAdvertising(base::OnceClosure on_stop_advertising_callback) override;

  void set_feature_support_status(FeatureSupportStatus feature_support_status) {
    feature_support_status_ = feature_support_status;
  }

  size_t num_start_advertising_calls() const {
    return num_start_advertising_calls_;
  }

  size_t num_stop_advertising_calls() const {
    return num_stop_advertising_calls_;
  }

  ConnectionLifecycleListener* connection_lifecycle_listener() const {
    return connection_lifecycle_listener_;
  }

  ResultCallback on_start_advertising_callback() {
    return std::move(on_start_advertising_callback_);
  }

  base::OnceClosure on_stop_advertising_callback() {
    return std::move(on_stop_advertising_callback_);
  }

 private:
  size_t num_start_advertising_calls_ = 0;
  size_t num_stop_advertising_calls_ = 0;
  FeatureSupportStatus feature_support_status_ =
      FeatureSupportStatus::kSupported;
  ConnectionLifecycleListener* connection_lifecycle_listener_ = nullptr;
  ResultCallback on_start_advertising_callback_;
  base::OnceClosure on_stop_advertising_callback_;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_FAKE_TARGET_DEVICE_CONNECTION_BROKER_H_
