// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_
#define CHROMEOS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS_FWUPD) FakeFwupdClient
    : public FwupdClient {
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

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_
