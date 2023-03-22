// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AUTOFILL_STATES_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AUTOFILL_STATES_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/prefs/pref_service.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {
class AutofillStatesComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit AutofillStatesComponentInstallerPolicy(PrefService* pref_service);
  ~AutofillStatesComponentInstallerPolicy() override;

  AutofillStatesComponentInstallerPolicy(
      const AutofillStatesComponentInstallerPolicy&) = delete;
  AutofillStatesComponentInstallerPolicy& operator=(
      const AutofillStatesComponentInstallerPolicy&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

#if defined(UNIT_TEST)
  bool VerifyInstallationForTesting(const base::Value::Dict& manifest,
                                    const base::FilePath& install_dir) {
    return VerifyInstallation(manifest, install_dir);
  }

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::Value::Dict manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }
#endif

 private:
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

  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
};

// Call once during startup to make the component update service aware of
// the File Type Policies component.
void RegisterAutofillStatesComponent(ComponentUpdateService* cus,
                                     PrefService* prefs);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AUTOFILL_STATES_COMPONENT_INSTALLER_H_
