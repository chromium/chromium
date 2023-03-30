// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_FAKE_SYSTEM_CLOCK_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_FAKE_SYSTEM_CLOCK_CLIENT_H_

#include <stdint.h>
#include <vector>

#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "dbus/object_proxy.h"

namespace ash {

// A fake implementation of SystemClockClient. This class does nothing.
class COMPONENT_EXPORT(SYSTEM_CLOCK) FakeSystemClockClient
    : public SystemClockClient,
      public SystemClockClient::TestInterface {
 public:
  FakeSystemClockClient();

  FakeSystemClockClient(const FakeSystemClockClient&) = delete;
  FakeSystemClockClient& operator=(const FakeSystemClockClient&) = delete;

  ~FakeSystemClockClient() override;

  // TestInterface
  void SetNetworkSynchronized(bool network_synchronized) override;
  void NotifyObserversSystemClockUpdated() override;
  void SetServiceIsAvailable(bool is_available) override;
  void DisableService() override;

  // SystemClockClient overrides
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void SetTime(int64_t time_in_seconds) override;
  bool CanSetTime() override;
  void GetLastSyncInfo(GetLastSyncInfoCallback callback) override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  SystemClockClient::TestInterface* GetTestInterface() override;

 private:
  bool is_available_ = true;
  bool is_enabled_ = true;
  bool network_synchronized_ = false;

  std::vector<dbus::ObjectProxy::WaitForServiceToBeAvailableCallback>
      callbacks_;
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SYSTEM_CLOCK_FAKE_SYSTEM_CLOCK_CLIENT_H_
