// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_
#define CHROMEOS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

namespace chromeos {

// A fake implementation of DlcserviceClient. The user of this class can
// use set_update_engine_client_status() to set a fake last Status and
// GetLastStatus() returns the fake with no modification. Other methods do
// nothing.
class COMPONENT_EXPORT(DLCSERVICE_CLIENT) FakeDlcserviceClient
    : public DlcserviceClient {
 public:
  FakeDlcserviceClient();
  ~FakeDlcserviceClient() override;

  // DlcserviceClient:
  void AddObserver(Observer* obs) override;
  void RemoveObserver(Observer* obs) override;
  void NotifyProgressUpdateForTest(double progress) override;
  void Install(const dlcservice::DlcModuleList& dlc_module_list,
               InstallCallback callback) override;
  void Uninstall(const std::string& dlc_id,
                 UninstallCallback callback) override;
  void GetInstalled(GetInstalledCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_
