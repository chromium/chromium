// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/arc/mojom/bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {
class BluetoothUUID;
}

namespace arc {

class FakeBluetoothInstance : public mojom::BluetoothInstance {
 public:
  class GattDBResult {
   public:
    GattDBResult(mojom::BluetoothAddressPtr&& remote_addr,
                 std::vector<mojom::BluetoothGattDBElementPtr>&& db);
    ~GattDBResult();

    const mojom::BluetoothAddressPtr& remote_addr() const {
      return remote_addr_;
    }

    const std::vector<mojom::BluetoothGattDBElementPtr>& db() const {
      return db_;
    }

   private:
    mojom::BluetoothAddressPtr remote_addr_;
    std::vector<mojom::BluetoothGattDBElementPtr> db_;

    DISALLOW_COPY_AND_ASSIGN(GattDBResult);
  };

  class LEDeviceFoundData {
   public:
    LEDeviceFoundData(mojom::BluetoothAddressPtr addr,
                      int32_t rssi,
                      std::vector<mojom::BluetoothAdvertisingDataPtr> adv_data,
                      const std::vector<uint8_t>& eir);
    ~LEDeviceFoundData();

    const mojom::BluetoothAddressPtr& addr() const { return addr_; }

    int32_t rssi() const { return rssi_; }

    const std::vector<mojom::BluetoothAdvertisingDataPtr>& adv_data() const {
      return adv_data_;
    }

    const std::vector<uint8_t>& eir() const { return eir_; }

   private:
    mojom::BluetoothAddressPtr addr_;
    int32_t rssi_;
    std::vector<mojom::BluetoothAdvertisingDataPtr> adv_data_;
    std::vector<uint8_t> eir_;

    DISALLOW_COPY_AND_ASSIGN(LEDeviceFoundData);
  };

  FakeBluetoothInstance();
  ~FakeBluetoothInstance() override;

  // mojom::BluetoothInstance overrides:
  void InitDeprecated(mojom::BluetoothHostPtr host_ptr) override;
  void Init(mojom::BluetoothHostPtr host_ptr, InitCallback callback) override;
  void OnAdapterProperties(
      mojom::BluetoothStatus status,
      std::vector<mojom::BluetoothPropertyPtr> properties) override;
  void OnDeviceFound(
      std::vector<mojom::BluetoothPropertyPtr> properties) override;
  void OnDiscoveryStateChanged(mojom::BluetoothDiscoveryState state) override;
  void OnBondStateChanged(mojom::BluetoothStatus status,
                          mojom::BluetoothAddressPtr remote_addr,
                          mojom::BluetoothBondState state) override;
  void OnLEDeviceFoundForN(
      mojom::BluetoothAddressPtr addr,
      int32_t rssi,
      std::vector<mojom::BluetoothAdvertisingDataPtr> adv_data) override;
  void OnLEDeviceFound(mojom::BluetoothAddressPtr addr,
                       int32_t rssi,
                       const std::vector<uint8_t>& eir) override;
  void OnLEConnectionStateChange(mojom::BluetoothAddressPtr remote_addr,
                                 bool connected) override;
  void OnLEDeviceAddressChange(mojom::BluetoothAddressPtr old_addr,
                               mojom::BluetoothAddressPtr new_addr) override;
  void OnSearchComplete(mojom::BluetoothAddressPtr remote_addr,
                        mojom::BluetoothGattStatus status) override;
  void OnGetGattDB(mojom::BluetoothAddressPtr remote_addr,
                   std::vector<mojom::BluetoothGattDBElementPtr> db) override;

  void OnGattNotify(mojom::BluetoothAddressPtr remote_addr,
                    mojom::BluetoothGattServiceIDPtr service_id,
                    mojom::BluetoothGattIDPtr char_id,
                    bool is_notify,
                    const std::vector<uint8_t>& value) override;

  void RequestGattRead(mojom::BluetoothAddressPtr address,
                       int32_t attribute_handle,
                       int32_t offset,
                       bool is_long,
                       mojom::BluetoothGattDBAttributeType attribute_type,
                       RequestGattReadCallback callback) override;

  void RequestGattWrite(mojom::BluetoothAddressPtr address,
                        int32_t attribute_handle,
                        int32_t offset,
                        const std::vector<uint8_t>& value,
                        mojom::BluetoothGattDBAttributeType attribute_type,
                        bool is_prepare,
                        RequestGattWriteCallback callback) override;

  void RequestGattExecuteWrite(
      mojom::BluetoothAddressPtr address,
      bool execute,
      RequestGattExecuteWriteCallback callback) override;

  void OnGetSdpRecords(
      mojom::BluetoothStatus status,
      mojom::BluetoothAddressPtr remote_addr,
      const device::BluetoothUUID& target_uuid,
      std::vector<mojom::BluetoothSdpRecordPtr> records) override;

  void OnMTUReceived(mojom::BluetoothAddressPtr remote_addr,
                     uint16_t mtu) override;

  const std::vector<std::vector<mojom::BluetoothPropertyPtr>>&
  device_found_data() const {
    return device_found_data_;
  }

  const std::vector<std::unique_ptr<LEDeviceFoundData>>& le_device_found_data()
      const {
    return le_device_found_data_;
  }

  const std::vector<std::unique_ptr<GattDBResult>>& gatt_db_result() const {
    return gatt_db_result_;
  }

 private:
  std::vector<std::vector<mojom::BluetoothPropertyPtr>> device_found_data_;
  std::vector<std::unique_ptr<LEDeviceFoundData>> le_device_found_data_;
  std::vector<std::unique_ptr<GattDBResult>> gatt_db_result_;

  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojom::BluetoothHostPtr host_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_BLUETOOTH_INSTANCE_H_
