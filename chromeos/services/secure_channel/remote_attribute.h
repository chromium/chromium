// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_

#include <string>

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace chromeos {

namespace secure_channel {

// Represents an attribute in the peripheral (service or characteristic).
struct RemoteAttribute {
  device::BluetoothUUID uuid;
  std::string id;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_REMOTE_ATTRIBUTE_H_
