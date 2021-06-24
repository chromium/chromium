// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_
#define CHROMEOS_COMPONENTS_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_

#include "base/observer_list.h"
#include "chromeos/components/quick_pair/common/device.h"
#include "chromeos/components/quick_pair/scanning/scanner_broker.h"

namespace chromeos {
namespace quick_pair {

class ScannerBrokerImpl : public ScannerBroker {
 public:
  ScannerBrokerImpl() = default;
  ScannerBrokerImpl(const ScannerBrokerImpl&) = delete;
  ScannerBrokerImpl& operator=(const ScannerBrokerImpl&) = delete;
  ~ScannerBrokerImpl() = default;

  // ScannerBroker:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void StartScanning(Protocol protocol) override;
  void StopScanning(Protocol protocol) override;

 private:
  void StartFastPairScanning();
  void StopFastPairScanning();

  void NotifyDeviceFound(Device device);
  void NotifyDeviceLost(Device device);

  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_PAIR_SCANNING_SCANNER_BROKER_IMPL_H_
