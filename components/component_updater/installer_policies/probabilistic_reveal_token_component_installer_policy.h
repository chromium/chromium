// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PROBABILISTIC_REVEAL_TOKEN_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PROBABILISTIC_REVEAL_TOKEN_COMPONENT_INSTALLER_POLICY_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ProbabilisticRevealTokenComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using RegistryReadyRepeatingCallback =
      base::RepeatingCallback<void(const std::optional<std::string>)>;

  explicit ProbabilisticRevealTokenComponentInstallerPolicy(
      RegistryReadyRepeatingCallback on_registry_ready);
  ProbabilisticRevealTokenComponentInstallerPolicy();
  ProbabilisticRevealTokenComponentInstallerPolicy(
      const ProbabilisticRevealTokenComponentInstallerPolicy&) = delete;
  ProbabilisticRevealTokenComponentInstallerPolicy& operator=(
      const ProbabilisticRevealTokenComponentInstallerPolicy&) = delete;
  ~ProbabilisticRevealTokenComponentInstallerPolicy() override;

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::Value::Dict manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }

  static base::FilePath GetInstalledPathForTesting(const base::FilePath& base);

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

  RegistryReadyRepeatingCallback on_registry_ready_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PROBABILISTIC_REVEAL_TOKEN_COMPONENT_INSTALLER_POLICY_H_
