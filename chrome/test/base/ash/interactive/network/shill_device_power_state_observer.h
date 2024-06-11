// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_NETWORK_SHILL_DEVICE_POWER_STATE_OBSERVER_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_NETWORK_SHILL_DEVICE_POWER_STATE_OBSERVER_H_

#include <string>

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "dbus/object_path.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

// This is a helper class that can be used in tests that use Kombucha to
// observe the enabled state of a Shill device.
class ShillDevicePowerStateObserver
    : public ui::test::ObservationStateObserver<bool,
                                                ShillManagerClient,
                                                ShillPropertyChangedObserver> {
 public:
  ShillDevicePowerStateObserver(ShillManagerClient* manager_client,
                                const NetworkTypePattern& type);
  ~ShillDevicePowerStateObserver() override;

 private:
  // ShillPropertyChangedObserver:
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  // ui::test::ObservationStateObserver:
  bool GetStateObserverInitialState() const override;

  bool IsDeviceEnabled() const;

  // The technology type to be observed.
  const NetworkTypePattern network_type_pattern_;
  bool device_enabled_state_;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_NETWORK_SHILL_DEVICE_POWER_STATE_OBSERVER_H_
