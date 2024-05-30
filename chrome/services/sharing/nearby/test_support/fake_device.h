// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_DEVICE_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_DEVICE_H_

#include "device/bluetooth/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace bluetooth {

class FakeDevice : public mojom::Device {
 public:
  FakeDevice();
  FakeDevice(const FakeDevice&) = delete;
  FakeDevice& operator=(const FakeDevice&) = delete;
  ~FakeDevice() override;

  // mojom::Device:
  void Disconnect() override;
  void GetInfo(GetInfoCallback callback) override;
  void GetServices(GetServicesCallback callback) override;
  void GetCharacteristics(const std::string& service_id,
                          GetCharacteristicsCallback callback) override;
  void ReadValueForCharacteristic(
      const std::string& service_id,
      const std::string& characteristic_id,
      ReadValueForCharacteristicCallback callback) override;
  void WriteValueForCharacteristic(
      const std::string& service_id,
      const std::string& characteristic_id,
      const std::vector<uint8_t>& value,
      WriteValueForCharacteristicCallback callback) override;
  void GetDescriptors(const std::string& service_id,
                      const std::string& characteristic_id,
                      GetDescriptorsCallback callback) override;
  void ReadValueForDescriptor(const std::string& service_id,
                              const std::string& characteristic_id,
                              const std::string& descriptor_id,
                              ReadValueForDescriptorCallback callback) override;
  void WriteValueForDescriptor(
      const std::string& service_id,
      const std::string& characteristic_id,
      const std::string& descriptor_id,
      const std::vector<uint8_t>& value,
      WriteValueForDescriptorCallback callback) override;

  void set_services(std::vector<bluetooth::mojom::ServiceInfoPtr> services) {
    services_ = std::move(services);
  }

  void set_characteristics(
      const std::string& service_id,
      std::optional<std::vector<bluetooth::mojom::CharacteristicInfoPtr>>
          characteristics) {
    service_id_to_characteristics_map_.insert_or_assign(
        service_id, std::move(characteristics));
  }

  void set_read_value_for_characteristic_response(
      bluetooth::mojom::GattResult result,
      const std::optional<std::vector<uint8_t>>& value) {
    read_value_gatt_result_ = result;
    read_value_ = value;
  }

  void set_on_disconnected_callback(
      base::OnceClosure on_disconnected_callback) {
    on_disconnected_callback_ = std::move(on_disconnected_callback);
  }

 private:
  base::OnceClosure on_disconnected_callback_;
  bool is_disconnected_ = false;
  std::vector<bluetooth::mojom::ServiceInfoPtr> services_;
  std::map<std::string,
           std::optional<std::vector<bluetooth::mojom::CharacteristicInfoPtr>>>
      service_id_to_characteristics_map_;
  bluetooth::mojom::GattResult read_value_gatt_result_;
  std::optional<std::vector<uint8_t>> read_value_;
  mojo::Receiver<mojom::Device> device_{this};
};

}  // namespace bluetooth

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_DEVICE_H_
