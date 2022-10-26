// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_FAILURE_TYPE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_FAILURE_TYPE_H_

#include <ostream>

namespace ash::secure_channel {

enum class BleInitiatorFailureType {
  // A connection was formed successfully, but there was an error
  // authenticating the connection.
  kAuthenticationError,

  // This device successfully picked up a scan result for the remote device, but
  // there was an error forming the connection.
  kGattConnectionError,

  // A higher-priority message needed to be sent, so this attempt needed to be
  // stopped temporarily.
  kInterruptedByHigherPriorityConnectionAttempt,

  // No scan result was ever discovered for the remote device.
  kTimeoutContactingRemoteDevice,

  // An advertisement could not be generated.
  kCouldNotGenerateAdvertisement
};

std::ostream& operator<<(std::ostream& stream,
                         const BleInitiatorFailureType& failure_type);

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_BLE_INITIATOR_FAILURE_TYPE_H_
