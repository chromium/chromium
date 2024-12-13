// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class AmountExtractionHeuristicRegexesInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  AmountExtractionHeuristicRegexesInstallerPolicy();

  AmountExtractionHeuristicRegexesInstallerPolicy(
      const AmountExtractionHeuristicRegexesInstallerPolicy&) = delete;
  AmountExtractionHeuristicRegexesInstallerPolicy& operator=(
      const AmountExtractionHeuristicRegexesInstallerPolicy&) = delete;

  ~AmountExtractionHeuristicRegexesInstallerPolicy() override;

 private:
  friend class AmountExtractionHeuristicRegexesInstallerPolicyTest;
  // The following methods override ComponentInstallerPolicy.
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

void RegisterAmountExtractionHeuristicRegexesComponent(
    ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_COMPONENT_INSTALLER_H_
