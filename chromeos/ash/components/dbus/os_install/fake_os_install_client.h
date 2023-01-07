// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_OS_INSTALL_FAKE_OS_INSTALL_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_OS_INSTALL_FAKE_OS_INSTALL_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/os_install/os_install_client.h"

namespace ash {

class COMPONENT_EXPORT(OS_INSTALL) FakeOsInstallClient
    : public OsInstallClient,
      public OsInstallClient::TestInterface {
 public:
  FakeOsInstallClient();
  ~FakeOsInstallClient() override;

  FakeOsInstallClient(const FakeOsInstallClient&) = delete;
  FakeOsInstallClient& operator=(const FakeOsInstallClient&) = delete;

  // OsInstallClient overrides
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  TestInterface* GetTestInterface() override;
  void StartOsInstall() override;

  // TestInterface overrides
  void UpdateStatus(Status status) override;

 private:
  void NotifyObservers(Status status, const std::string& service_log);

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_OS_INSTALL_FAKE_OS_INSTALL_CLIENT_H_
