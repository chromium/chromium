// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_PAIR_SCANNING_SCANNER_BROKER_H_
#define CHROMEOS_COMPONENTS_QUICK_PAIR_SCANNING_SCANNER_BROKER_H_

#include "base/observer_list_types.h"
#include "chromeos/components/quick_pair/common/device.h"
#include "chromeos/components/quick_pair/common/logging.h"
#include "chromeos/components/quick_pair/common/protocol.h"

namespace chromeos {
namespace quick_pair {

// The ScannerBroker is the entry point for the Scanning component in the Quick
// Pair system. It is responsible for brokering the start/stop scanning calls
// to the correct concrete Scanner implementation, and exposing an observer
// pattern for other components to become aware of device found/lost events.
class ScannerBroker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceFound(Device device);
    virtual void OnDeviceLost(Device device);
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void StartScanning(Protocol protocol) = 0;
  virtual void StopScanning(Protocol protocol) = 0;
};

}  // namespace quick_pair
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_PAIR_SCANNING_SCANNER_BROKER_H_
