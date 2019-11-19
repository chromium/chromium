// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SYSTEM_CLOCK_FAKE_SYSTEM_CLOCK_CLIENT_H_
#define CHROMEOS_DBUS_SYSTEM_CLOCK_FAKE_SYSTEM_CLOCK_CLIENT_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// A fake implementation of SystemClockClient. This class does nothing.
class COMPONENT_EXPORT(SYSTEM_CLOCK) FakeSystemClockClient
    : public SystemClockClient,
      public SystemClockClient::TestInterface {
 public:
  FakeSystemClockClient();
  ~FakeSystemClockClient() override;

  // TestInterface
  void SetNetworkSynchronized(bool network_synchronized) override;
  void NotifyObserversSystemClockUpdated() override;
  void SetServiceIsAvailable(bool is_available) override;

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
  bool network_synchronized_ = false;

  std::vector<dbus::ObjectProxy::WaitForServiceToBeAvailableCallback>
      callbacks_;
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeSystemClockClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SYSTEM_CLOCK_FAKE_SYSTEM_CLOCK_CLIENT_H_
