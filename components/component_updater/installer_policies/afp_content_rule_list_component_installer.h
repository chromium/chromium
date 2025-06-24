// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AFP_CONTENT_RULE_LIST_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AFP_CONTENT_RULE_LIST_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// ComponentInstallerPolicy for the Anti-Fingerprinting Content Rule List.
class AntiFingerprintingContentRuleListComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using OnLoadCompleteCallback =
      base::RepeatingCallback<void(std::optional<std::string>)>;

  static constexpr char kExperimentalVersionAttributeName[] =
      "_experimental_list_version";

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(InstallationResult)
  enum class InstallationResult {
    kSuccess = 0,
    kMissingJsonFile = 1,
    kMaxValue = kMissingJsonFile,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:FingerprintingProtectionWKComponentInstallationResult)

  explicit AntiFingerprintingContentRuleListComponentInstallerPolicy(
      OnLoadCompleteCallback on_load_complete);
  ~AntiFingerprintingContentRuleListComponentInstallerPolicy() override;

 private:
  friend class AntiFingerprintingContentRuleListComponentInstallerPolicyTest;

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

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  OnLoadCompleteCallback on_load_complete_;
};

void RegisterAntiFingerprintingContentRuleListComponent(
    ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AFP_CONTENT_RULE_LIST_COMPONENT_INSTALLER_H_
