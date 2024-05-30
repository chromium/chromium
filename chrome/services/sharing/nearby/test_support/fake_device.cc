// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/fake_device.h"

namespace bluetooth {

FakeDevice::FakeDevice() = default;

FakeDevice::~FakeDevice() = default;

void FakeDevice::Disconnect() {
  is_disconnected_ = true;
  if (on_disconnected_callback_) {
    std::move(on_disconnected_callback_).Run();
  }
}

void FakeDevice::GetInfo(GetInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeDevice::GetServices(GetServicesCallback callback) {
  std::move(callback).Run(std::move(services_));
}

void FakeDevice::GetCharacteristics(const std::string& service_id,
                                    GetCharacteristicsCallback callback) {
  std::move(callback).Run(
      std::move(service_id_to_characteristics_map_.at(service_id)));
}

void FakeDevice::ReadValueForCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_id,
    ReadValueForCharacteristicCallback callback) {
  std::move(callback).Run(read_value_gatt_result_, read_value_);
}

void FakeDevice::WriteValueForCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::vector<uint8_t>& value,
    WriteValueForCharacteristicCallback callback) {
  NOTIMPLEMENTED();
}

void FakeDevice::GetDescriptors(const std::string& service_id,
                                const std::string& characteristic_id,
                                GetDescriptorsCallback callback) {
  NOTIMPLEMENTED();
}

void FakeDevice::ReadValueForDescriptor(
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::string& descriptor_id,
    ReadValueForDescriptorCallback callback) {
  NOTIMPLEMENTED();
}

void FakeDevice::WriteValueForDescriptor(
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::string& descriptor_id,
    const std::vector<uint8_t>& value,
    WriteValueForDescriptorCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace bluetooth
