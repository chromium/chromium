// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

// This file name must be in sync with the server-side configuration, or updates
// will fail.
const base::FilePath::CharType kTrustTokenKeyCommitmentsFileName[] =
    FILE_PATH_LITERAL("keys.json");

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

  // Returns the component's SHA2 hash as raw bytes.
  static void GetPublicKeyHash(std::vector<uint8_t>* hash);

  // Loads trust tokens from string using the given `load_keys_from_disk` call.
  //
  // static to allow sharing with the Android `ComponentLoaderPolicy`.
  //
  // `load_keys_from_disk` a callback that read trust tokens from file and
  // return them as an optional string. `on_commitments_ready` loads trust
  // tokens in network service.
  static void LoadTrustTokensFromString(
      base::OnceCallback<std::optional<std::string>()> load_keys_from_disk,
      base::OnceCallback<void(const std::string&)> on_commitments_ready);

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
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  base::RepeatingCallback<void(const std::string&)> on_commitments_ready_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_TRUST_TOKEN_KEY_COMMITMENTS_COMPONENT_INSTALLER_POLICY_H_
