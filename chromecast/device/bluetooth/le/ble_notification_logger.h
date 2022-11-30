// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_BLE_NOTIFICATION_LOGGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_BLE_NOTIFICATION_LOGGER_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"

namespace chromecast {
namespace bluetooth {

class BleNotificationLogger : public GattClientManager::Observer {
 public:
  static constexpr auto kMinLogInterval = base::Minutes(1);

  explicit BleNotificationLogger(GattClientManager* gcm);

  BleNotificationLogger(const BleNotificationLogger&) = delete;
  BleNotificationLogger& operator=(const BleNotificationLogger&) = delete;

  ~BleNotificationLogger() override;

  // GattClientManager::Observer implementation:
  void OnCharacteristicNotification(
      scoped_refptr<RemoteDevice> device,
      scoped_refptr<RemoteCharacteristic> characteristic,
      std::vector<uint8_t> value) override;

 private:
  using Addr = bluetooth_v2_shlib::Addr;
  using Uuid = bluetooth_v2_shlib::Uuid;

  void MaybeLogHistogramState();

  SEQUENCE_CHECKER(sequence_checker_);

  GattClientManager* const gcm_;

  base::TimeTicks last_log_time_;
  base::OneShotTimer log_timer_;

  // Key: Device address, Value: Map[Key: Characteristic UUID, Value: count]
  std::map<Addr, std::map<Uuid, int32_t>> device_to_char_uuid_to_count_;
  base::WeakPtrFactory<BleNotificationLogger> weak_factory_;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  //  CHROMECAST_DEVICE_BLUETOOTH_LE_BLE_NOTIFICATION_LOGGER_H_
