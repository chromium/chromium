// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_OPEN_COOKIE_DATABASE_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_OPEN_COOKIE_DATABASE_COMPONENT_INSTALLER_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class OpenCookieDatabaseComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  OpenCookieDatabaseComponentInstallerPolicy() = default;
  OpenCookieDatabaseComponentInstallerPolicy(
      const OpenCookieDatabaseComponentInstallerPolicy&) = delete;
  OpenCookieDatabaseComponentInstallerPolicy& operator=(
      const OpenCookieDatabaseComponentInstallerPolicy&) = delete;
  ~OpenCookieDatabaseComponentInstallerPolicy() override = default;

  // ComponentInstallerPolicy:
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;

 private:
  // ComponentInstallerPolicy:
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
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_OPEN_COOKIE_DATABASE_COMPONENT_INSTALLER_POLICY_H_
