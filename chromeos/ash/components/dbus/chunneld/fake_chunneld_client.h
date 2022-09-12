// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CHUNNELD_FAKE_CHUNNELD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CHUNNELD_FAKE_CHUNNELD_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"

namespace ash {

// FakeChunneldClient is a stub implementation of ChunneldClient used for
// testing.
class COMPONENT_EXPORT(ASH_DBUS_CHUNNELD) FakeChunneldClient
    : public ChunneldClient {
 public:
  FakeChunneldClient();
  ~FakeChunneldClient() override;
  FakeChunneldClient(const FakeChunneldClient&) = delete;
  FakeChunneldClient& operator=(const FakeChunneldClient&) = delete;

  // ChunneldClient:
  void Init(dbus::Bus* bus) override {}
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  void NotifyChunneldStopped();
  void NotifyChunneldStarted();

 private:
  void InitializeProtoResponses();

  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CHUNNELD_FAKE_CHUNNELD_CLIENT_H_
