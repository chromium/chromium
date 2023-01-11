// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"

namespace ash::secure_channel {

// Advertises to the device with the given ID. Due to issues in the Bluetooth
// stack, it is possible that registering or unregistering an advertisement can
// fail. If this class encounters an error, it retries until it succeeds. Once
// Stop() is called, the advertisement should not be considered unregistered
// until the stop callback is invoked.
class ErrorTolerantBleAdvertisement {
 public:
  ErrorTolerantBleAdvertisement(const ErrorTolerantBleAdvertisement&) = delete;
  ErrorTolerantBleAdvertisement& operator=(
      const ErrorTolerantBleAdvertisement&) = delete;

  virtual ~ErrorTolerantBleAdvertisement();

  // Stops advertising. Because BLE advertisements start and stop
  // asynchronously, clients must use this function to stop advertising instead
  // of simply deleting an ErrorTolerantBleAdvertisement object. Clients should
  // not assume that advertising has actually stopped until |callback| has been
  // invoked.
  virtual void Stop(base::OnceClosure callback) = 0;

  // Returns whether Stop() has been called.
  virtual bool HasBeenStopped() = 0;

  const DeviceIdPair& device_id_pair() const { return device_id_pair_; }

 protected:
  ErrorTolerantBleAdvertisement(const DeviceIdPair& device_id_pair);

 private:
  const DeviceIdPair device_id_pair_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_
