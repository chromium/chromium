// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"

namespace ash::quick_start {

FakeTargetDeviceConnectionBroker::Factory::Factory() = default;

FakeTargetDeviceConnectionBroker::Factory::~Factory() = default;

std::unique_ptr<TargetDeviceConnectionBroker>
FakeTargetDeviceConnectionBroker::Factory::CreateInstance() {
  auto connection_broker = std::make_unique<FakeTargetDeviceConnectionBroker>();
  instances_.push_back(connection_broker.get());
  return std::move(connection_broker);
}

FakeTargetDeviceConnectionBroker::FakeTargetDeviceConnectionBroker() = default;

FakeTargetDeviceConnectionBroker::~FakeTargetDeviceConnectionBroker() = default;

TargetDeviceConnectionBroker::FeatureSupportStatus
FakeTargetDeviceConnectionBroker::GetFeatureSupportStatus() const {
  return feature_support_status_;
}

void FakeTargetDeviceConnectionBroker::StartAdvertising(
    ConnectionLifecycleListener* listener,
    ResultCallback on_start_advertising_callback) {
  ++num_start_advertising_calls_;
  connection_lifecycle_listener_ = listener;
  on_start_advertising_callback_ = std::move(on_start_advertising_callback);
}

void FakeTargetDeviceConnectionBroker::StopAdvertising(
    base::OnceClosure on_stop_advertising_callback) {
  ++num_stop_advertising_calls_;
  on_stop_advertising_callback_ = std::move(on_stop_advertising_callback);
}

}  // namespace ash::quick_start
