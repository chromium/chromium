// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/ble_notification_logger.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_device.h"

namespace chromecast {
namespace bluetooth {

// static
constexpr base::TimeDelta BleNotificationLogger::kMinLogInterval;

BleNotificationLogger::BleNotificationLogger(GattClientManager* gcm)
    : gcm_(gcm), weak_factory_(this) {
  DCHECK(gcm);
  gcm_->AddObserver(this);
}

BleNotificationLogger::~BleNotificationLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gcm_->RemoveObserver(this);
}

void BleNotificationLogger::OnCharacteristicNotification(
    scoped_refptr<RemoteDevice> device,
    scoped_refptr<RemoteCharacteristic> characteristic,
    std::vector<uint8_t> value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++device_to_char_uuid_to_count_[device->addr()][characteristic->uuid()];

  MaybeLogHistogramState();
}

void BleNotificationLogger::MaybeLogHistogramState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (device_to_char_uuid_to_count_.empty()) {
    return;
  }

  auto now = base::TimeTicks::Now();
  base::TimeTicks next_time_can_log = last_log_time_ + kMinLogInterval;
  if (now < next_time_can_log) {
    auto time_till_next_log = next_time_can_log - now;
    // Schedule a log at the next time we are allowed to.
    log_timer_.Start(
        FROM_HERE, time_till_next_log,
        base::BindOnce(&BleNotificationLogger::MaybeLogHistogramState,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  LOG(INFO) << "BLE notifications: ";
  for (const auto& device : device_to_char_uuid_to_count_) {
    LOG(INFO) << util::AddrLastByteString(device.first);
    for (const auto& characteristic : device.second) {
      LOG(INFO) << "  " << util::UuidToString(characteristic.first) << " "
                << characteristic.second;
    }
  }

  device_to_char_uuid_to_count_.clear();
  last_log_time_ = now;
}

}  // namespace bluetooth
}  // namespace chromecast
