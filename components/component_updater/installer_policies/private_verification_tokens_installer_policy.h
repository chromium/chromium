// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PRIVATE_VERIFICATION_TOKENS_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PRIVATE_VERIFICATION_TOKENS_INSTALLER_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"

namespace component_updater {

inline constexpr base::FilePath::CharType kPvtConfigFileName[] =
    FILE_PATH_LITERAL("pvt_config.json");

// Policy for installing the Private Verification Tokens component.
class PrivateVerificationTokensInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  // Callback invoked when the component is ready and parsed.
  // It passes the parsed config.
  using OnComponentReadyCallback = base::RepeatingCallback<void(
      scoped_refptr<
          private_verification_tokens::PrivateVerificationTokensIssuerConfig>)>;

  explicit PrivateVerificationTokensInstallerPolicy(
      OnComponentReadyCallback callback);
  ~PrivateVerificationTokensInstallerPolicy() override;

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::DictValue manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }

  PrivateVerificationTokensInstallerPolicy(
      const PrivateVerificationTokensInstallerPolicy&) = delete;
  PrivateVerificationTokensInstallerPolicy& operator=(
      const PrivateVerificationTokensInstallerPolicy&) = delete;

  // Returns the component's SHA2 hash as raw bytes.
  static void GetPublicKeyHash(std::vector<uint8_t>* hash);

 private:
  // The following methods override ComponentInstallerPolicy.
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

  OnComponentReadyCallback callback_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_PRIVATE_VERIFICATION_TOKENS_INSTALLER_POLICY_H_
