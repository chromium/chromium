// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_FACTORY_H_

#include <memory>

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker.h"

namespace ash::quick_start {

// A factory class for creating instances of TargetDeviceConnectionBroker.
// Calling code should use the static Create() method.
class TargetDeviceConnectionBrokerFactory {
 public:
  static std::unique_ptr<TargetDeviceConnectionBroker> Create();

  static void SetFactoryForTesting(
      TargetDeviceConnectionBrokerFactory* test_factory);

  TargetDeviceConnectionBrokerFactory();
  TargetDeviceConnectionBrokerFactory(TargetDeviceConnectionBrokerFactory&) =
      delete;
  TargetDeviceConnectionBrokerFactory& operator=(
      TargetDeviceConnectionBrokerFactory&) = delete;
  virtual ~TargetDeviceConnectionBrokerFactory();

 protected:
  virtual std::unique_ptr<TargetDeviceConnectionBroker> CreateInstance() = 0;

 private:
  static TargetDeviceConnectionBrokerFactory* test_factory_;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_FACTORY_H_
