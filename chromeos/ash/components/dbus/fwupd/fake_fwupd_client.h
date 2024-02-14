// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
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

  void TriggerPropertiesChangeForTesting(uint32_t percentage,
                                         uint32_t status) override;
  void TriggerSuccessfulUpdateForTesting() override;
  bool HasUpdateStartedForTesting() override;
  void EmitDeviceRequestForTesting(uint32_t device_request_id) override;

 private:
  void SetFwupdFeatureFlags() override;

  // True if InstallUpdate has been called.
  bool has_update_started_ = false;

  // The temporary directory where fake update files are created.
  base::ScopedTempDir temp_directory_;

  // Creates a fake update file (with .cab extension) in a temporary directory.
  base::FilePath CreateUpdateFilePath();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FWUPD_FAKE_FWUPD_CLIENT_H_
