// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class OriginTrialsComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  static void GetComponentHash(std::vector<uint8_t>* hash);
  OriginTrialsComponentInstallerPolicy() = default;
  ~OriginTrialsComponentInstallerPolicy() override = default;
  OriginTrialsComponentInstallerPolicy(
      const OriginTrialsComponentInstallerPolicy&) = delete;
  OriginTrialsComponentInstallerPolicy& operator=(
      const OriginTrialsComponentInstallerPolicy&) = delete;

 protected:
  void GetHash(std::vector<uint8_t>* hash) const override;

 private:
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_ORIGIN_TRIALS_COMPONENT_INSTALLER_H_
