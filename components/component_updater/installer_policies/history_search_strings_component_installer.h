// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_HISTORY_SEARCH_STRINGS_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_HISTORY_SEARCH_STRINGS_COMPONENT_INSTALLER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

inline constexpr char kHistorySearchStringsBinaryPbFileName[] =
    "history_search_strings_farmhashed.binarypb";

// The installer policy implementation used by ComponentUpdateService to update
// the strings used by HistoryEmbeddingsService.
class HistorySearchStringsComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  HistorySearchStringsComponentInstallerPolicy();
  ~HistorySearchStringsComponentInstallerPolicy() override;
  HistorySearchStringsComponentInstallerPolicy(
      const HistorySearchStringsComponentInstallerPolicy&) = delete;
  HistorySearchStringsComponentInstallerPolicy& operator=(
      const HistorySearchStringsComponentInstallerPolicy&) = delete;

 private:
  friend class HistorySearchStringsComponentInstallerPolicyPublic;

  // ComponentInstallerPolicy:
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
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// Called once during startup to register this component with `cus`.
void RegisterHistorySearchStringsComponent(ComponentUpdateService* cus);

// Called to clean up any remaining state from this component.
void DeleteHistorySearchStringsComponent(const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_HISTORY_SEARCH_STRINGS_COMPONENT_INSTALLER_H_
