// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

// TrustTokenKeyCommitmentsComponentInstallerPolicy defines an installer
// responsible for receiving updated Trust Tokens
// (https://github.com/wicg/trust-token-api) key commitments and passing them to
// the network service via Mojo.
class TrustTokenKeyCommitmentsComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  // |on_commitments_ready| will be called on the UI thread when
  // key commitments become ready; it's exposed for testing.
  explicit TrustTokenKeyCommitmentsComponentInstallerPolicy(
      base::RepeatingCallback<void(const std::string&)> on_commitments_ready);
  ~TrustTokenKeyCommitmentsComponentInstallerPolicy() override;

  TrustTokenKeyCommitmentsComponentInstallerPolicy(
      const TrustTokenKeyCommitmentsComponentInstallerPolicy&) = delete;
  TrustTokenKeyCommitmentsComponentInstallerPolicy& operator=(
      const TrustTokenKeyCommitmentsComponentInstallerPolicy&) = delete;

 protected:
  void GetHash(std::vector<uint8_t>* hash) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TrustTokenKeyCommitmentsComponentInstallerTest,
                           LoadsCommitments);
  FRIEND_TEST_ALL_PREFIXES(TrustTokenKeyCommitmentsComponentInstallerTest,
                           LoadsCommitmentsFromOverriddenPath);

  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  base::RepeatingCallback<void(const std::string&)> on_commitments_ready_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_
