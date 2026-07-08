// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_POLICY_H_

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"

namespace base {
class Version;
}  // namespace base

namespace web_app {
class IwaKeyDistributionInfoProvider;
}  // namespace web_app

namespace component_updater {

inline constexpr char kIwaKeyDistributionComponentExpCohort[] =
    "iwa-kd-component-exp-cohort";

class IwaKeyDistributionComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  static constexpr base::FilePath::CharType kRelativeInstallDirName[] =
      FILE_PATH_LITERAL("IwaKeyDistribution");
  static constexpr char kManifestName[] = "Iwa Key Distribution";
  static constexpr base::FilePath::CharType kDataFileName[] =
      FILE_PATH_LITERAL("iwa-key-distribution.pb");
  // The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
  // The extension id is: iebhnlpddlcpcfpfalldikcoeakpeoah
  static constexpr std::array<uint8_t, 32> kIwaKeyDistributionPublicKeySHA256 =
      {0x84, 0x17, 0xdb, 0xf3, 0x3b, 0x2f, 0x25, 0xf5, 0x0b, 0xb3, 0x8a,
       0x2e, 0x40, 0xaf, 0x4e, 0x07, 0x18, 0xfa, 0xae, 0x6e, 0x0e, 0xdb,
       0x46, 0xfc, 0xc9, 0x36, 0x50, 0xcf, 0x38, 0xfa, 0xf9, 0xab};

  explicit IwaKeyDistributionComponentInstallerPolicy(
      base::RepeatingCallback<
          void(base::PassKey<web_app::IwaKeyDistributionInfoProvider>)>
          queue_on_demand_update);
  ~IwaKeyDistributionComponentInstallerPolicy() override = default;

  IwaKeyDistributionComponentInstallerPolicy(
      const IwaKeyDistributionComponentInstallerPolicy&) = delete;
  IwaKeyDistributionComponentInstallerPolicy operator=(
      const IwaKeyDistributionComponentInstallerPolicy&) = delete;

  static void QueueOnDemandUpdate(OnDemandUpdater& updater);

 private:
  // ComponentInstallerPolicy:
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_IWA_KEY_DISTRIBUTION_COMPONENT_INSTALLER_POLICY_H_
