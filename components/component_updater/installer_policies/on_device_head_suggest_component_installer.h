// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_ON_DEVICE_HEAD_SUGGEST_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_ON_DEVICE_HEAD_SUGGEST_COMPONENT_INSTALLER_H_

#include <vector>

#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// The installer policy implementation which is required by component updater in
// order to update the on device model used by Omnibox suggest provider
// OnDeviceHeadProvider.
class OnDeviceHeadSuggestInstallerPolicy : public ComponentInstallerPolicy {
 public:
  OnDeviceHeadSuggestInstallerPolicy(const std::string& locale);

  OnDeviceHeadSuggestInstallerPolicy(
      const OnDeviceHeadSuggestInstallerPolicy&) = delete;
  OnDeviceHeadSuggestInstallerPolicy& operator=(
      const OnDeviceHeadSuggestInstallerPolicy&) = delete;

  ~OnDeviceHeadSuggestInstallerPolicy() override;

 private:
  // ComponentInstallerPolicy implementation.
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

  // The application (normalized) locale when this policy is created. Models
  // which do not match this locale will be rejected.
  std::string accept_locale_;
};

// Registers an OnDeviceHeadSuggest component with |cus|.
void RegisterOnDeviceHeadSuggestComponent(ComponentUpdateService* cus,
                                          const std::string& locale);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_ON_DEVICE_HEAD_SUGGEST_COMPONENT_INSTALLER_H_
