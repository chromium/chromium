// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/scanning/scanner_broker_impl.h"

#include "chromeos/components/quick_pair/common/device.h"
#include "chromeos/components/quick_pair/common/logging.h"
#include "chromeos/components/quick_pair/common/protocol.h"

namespace chromeos {
namespace quick_pair {

void ScannerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScannerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScannerBrokerImpl::StartScanning(Protocol protocol) {
  switch (protocol) {
    case Protocol::kFastPair:
      StartFastPairScanning();
      break;
  }
}

void ScannerBrokerImpl::StopScanning(Protocol protocol) {
  switch (protocol) {
    case Protocol::kFastPair:
      StopFastPairScanning();
      break;
  }
}

void ScannerBrokerImpl::StartFastPairScanning() {
  QP_LOG(INFO) << "Starting Fast Pair Scanning.";
}

void ScannerBrokerImpl::StopFastPairScanning() {
  QP_LOG(INFO) << "Stoping Fast Pair Scanning.";
}

void ScannerBrokerImpl::NotifyDeviceFound(Device device) {
  for (auto& observer : observers_) {
    observer.OnDeviceFound(device);
  }
}

void ScannerBrokerImpl::NotifyDeviceLost(Device device) {
  for (auto& observer : observers_) {
    observer.OnDeviceLost(device);
  }
}

}  // namespace quick_pair
}  // namespace chromeos
