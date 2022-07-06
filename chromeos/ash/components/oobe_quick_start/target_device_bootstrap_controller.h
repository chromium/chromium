// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker.h"

namespace ash::quick_start {

class TargetDeviceConnectionBroker;

class TargetDeviceBootstrapController
    : public TargetDeviceConnectionBroker::ConnectionLifecycleListener {
 public:
  TargetDeviceBootstrapController();
  TargetDeviceBootstrapController(TargetDeviceBootstrapController&) = delete;
  TargetDeviceBootstrapController& operator=(TargetDeviceBootstrapController&) =
      delete;
  ~TargetDeviceBootstrapController();

  struct Status {
    // TBD.
  };

  class Observer : public base::CheckedObserver {
    virtual void OnStatusChanged(const Status& status) = 0;

   protected:
    ~Observer() override = default;
  };

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // This function would crash (if DCHECKs are on) in case there are existing
  // valid weakptrs.
  base::WeakPtr<TargetDeviceBootstrapController> GetAsWeakPtrForClient();

  // TODO: Finalize api for frontend.
  void StartAdvertising();
  void StopAdvertising();

 private:
  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;

  // TODO: Should we enforce one observer at a time here too?
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<TargetDeviceBootstrapController>
      weak_ptr_factory_for_clients_{this};
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
