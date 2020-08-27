// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/device_id_pair.h"

namespace chromeos {

namespace secure_channel {

// Advertises to the device with the given ID. Due to issues in the Bluetooth
// stack, it is possible that registering or unregistering an advertisement can
// fail. If this class encounters an error, it retries until it succeeds. Once
// Stop() is called, the advertisement should not be considered unregistered
// until the stop callback is invoked.
class ErrorTolerantBleAdvertisement {
 public:
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

  DISALLOW_COPY_AND_ASSIGN(ErrorTolerantBleAdvertisement);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_ERROR_TOLERANT_BLE_ADVERTISEMENT_H_
