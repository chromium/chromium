// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PLUS_ADDRESS_BLOCKLIST_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PLUS_ADDRESS_BLOCKLIST_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class PlusAddressBlocklistInstallerPolicy : public ComponentInstallerPolicy {
 public:
  PlusAddressBlocklistInstallerPolicy();

  PlusAddressBlocklistInstallerPolicy(
      const PlusAddressBlocklistInstallerPolicy&) = delete;
  PlusAddressBlocklistInstallerPolicy& operator=(
      const PlusAddressBlocklistInstallerPolicy&) = delete;

  ~PlusAddressBlocklistInstallerPolicy() override;

 private:
  friend class PlusAddressBlocklistInstallerPolicyTest;
  // ComponentInstallerPolicy:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

void RegisterPlusAddressBlocklistComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PLUS_ADDRESS_BLOCKLIST_COMPONENT_INSTALLER_H_
