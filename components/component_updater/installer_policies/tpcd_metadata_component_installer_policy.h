// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TPCD_METADATA_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TPCD_METADATA_COMPONENT_INSTALLER_POLICY_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "components/update_client/update_client.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

inline constexpr base::FilePath::CharType kTpcdMetadataComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");
class TpcdMetadataComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  using OnTpcdMetadataComponentReadyCallback =
      base::RepeatingCallback<void(const std::string&)>;

  explicit TpcdMetadataComponentInstallerPolicy(
      OnTpcdMetadataComponentReadyCallback on_component_ready_callback);
  ~TpcdMetadataComponentInstallerPolicy() override;

  TpcdMetadataComponentInstallerPolicy(
      const TpcdMetadataComponentInstallerPolicy&) = delete;
  TpcdMetadataComponentInstallerPolicy& operator=(
      const TpcdMetadataComponentInstallerPolicy&) = delete;

  // Returns the component's SHA2 hash as raw bytes.
  static void GetPublicKeyHash(std::vector<uint8_t>* hash);

 private:
  FRIEND_TEST_ALL_PREFIXES(TpcdMetadataComponentInstallerPolicyTest,
                           VerifyAttributes);

  // Start of ComponentInstallerPolicy overrides:
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
  // End of ComponentInstallerPolicy overrides.

  static base::FilePath GetInstalledFilePath(const base::FilePath& base);
  void MaybeFireCallback(
      const std::optional<std::string>& maybe_classifications);

  base::FilePath installed_file_path_;

  OnTpcdMetadataComponentReadyCallback on_component_ready_callback_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TPCD_METADATA_COMPONENT_INSTALLER_POLICY_H_
