// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_CLIENT_SIDE_PHISHING_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_CLIENT_SIDE_PHISHING_COMPONENT_INSTALLER_POLICY_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace component_updater {

extern const base::FilePath::CharType kClientModelBinaryPbFileName[];
extern const base::FilePath::CharType kVisualTfLiteModelFileName[];

class ClientSidePhishingComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  // A callback to read model files from the given install path and populate the
  // model appropriately, used to customize the behaviour of `ComponentReady`.
  using ReadFilesCallback =
      base::RepeatingCallback<void(const base::FilePath&)>;
  // A callback that returns the appoperiate installer attributes, used to
  // customize the behaviour of `GetInstallerAttributes`.
  using InstallerAttributesCallback =
      base::RepeatingCallback<update_client::InstallerAttributes()>;

  ClientSidePhishingComponentInstallerPolicy(
      const ReadFilesCallback& read_files_callback,
      const InstallerAttributesCallback& installer_attributes_callback);
  ClientSidePhishingComponentInstallerPolicy(
      const ClientSidePhishingComponentInstallerPolicy&) = delete;
  ClientSidePhishingComponentInstallerPolicy& operator=(
      const ClientSidePhishingComponentInstallerPolicy&) = delete;
  ~ClientSidePhishingComponentInstallerPolicy() override;

  static void GetPublicHash(std::vector<uint8_t>* hash);

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

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  ReadFilesCallback read_files_callback_;
  InstallerAttributesCallback installer_attributes_callback_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_CLIENT_SIDE_PHISHING_COMPONENT_INSTALLER_POLICY_H_
