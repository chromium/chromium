// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_OPTIMIZATION_HINTS_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_OPTIMIZATION_HINTS_COMPONENT_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// Component for receiving Chrome Optimization Hints.
class OptimizationHintsComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  static const char kManifestRulesetFormatKey[];

  OptimizationHintsComponentInstallerPolicy();

  OptimizationHintsComponentInstallerPolicy(
      const OptimizationHintsComponentInstallerPolicy&) = delete;
  OptimizationHintsComponentInstallerPolicy& operator=(
      const OptimizationHintsComponentInstallerPolicy&) = delete;

  ~OptimizationHintsComponentInstallerPolicy() override;

 private:
  friend class OptimizationHintsComponentInstallerTest;

  // ComponentInstallerPolicy implementation.
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

  const base::Version ruleset_format_version_;
};

void RegisterOptimizationHintsComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_OPTIMIZATION_HINTS_COMPONENT_INSTALLER_H_
