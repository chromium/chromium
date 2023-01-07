// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"

namespace ash {

class COMPONENT_EXPORT(ASH_DBUS_FWUPD) FakeFwupdClient : public FwupdClient {
 public:
  FakeFwupdClient();
  FakeFwupdClient(const FakeFwupdClient&) = delete;
  FakeFwupdClient& operator=(const FakeFwupdClient&) = delete;
  ~FakeFwupdClient() override;

  // FwupdClient:
  void Init(dbus::Bus* bus) override;
  void RequestUpdates(const std::string& device_id) override;
  void RequestDevices() override;
  void InstallUpdate(const std::string& device_id,
                     base::ScopedFD file_descriptor,
                     FirmwareInstallOptions options) override;

 private:
  bool install_success_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_
