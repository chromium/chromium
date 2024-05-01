// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_FIRST_PARTY_SETS_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_FIRST_PARTY_SETS_COMPONENT_INSTALLER_POLICY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

inline constexpr base::FilePath::CharType kFpsMetadataComponentFileName[] =
    FILE_PATH_LITERAL("sets.json");

class FirstPartySetsComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  using SetsReadyOnceCallback =
      base::OnceCallback<void(base::Version, base::File)>;

  // `on_sets_ready` will be called on the UI thread when the sets are ready. It
  // is exposed here for testing.
  explicit FirstPartySetsComponentInstallerPolicy(
      SetsReadyOnceCallback on_sets_ready,
      base::TaskPriority priority);
  ~FirstPartySetsComponentInstallerPolicy() override;

  FirstPartySetsComponentInstallerPolicy(
      const FirstPartySetsComponentInstallerPolicy&) = delete;
  FirstPartySetsComponentInstallerPolicy operator=(
      const FirstPartySetsComponentInstallerPolicy&) = delete;

  void OnRegistrationComplete();

  // Resets static state. Should only be used to clear state during testing.
  static void ResetForTesting();

  // Seeds a component at `install_dir` with the given `contents`. Only to be
  // used in testing.
  static void WriteComponentForTesting(base::Version version,
                                       const base::FilePath& install_dir,
                                       std::string_view contents);

  static base::FilePath GetInstalledPathForTesting(const base::FilePath& base) {
    return GetInstalledPath(base);
  }

  void ComponentReadyForTesting(const base::Version& version,
                                const base::FilePath& install_dir,
                                base::Value::Dict manifest) {
    ComponentReady(version, install_dir, std::move(manifest));
  }

  update_client::InstallerAttributes GetInstallerAttributesForTesting() const {
    return GetInstallerAttributes();
  }

  // Returns the component's SHA2 hash as raw bytes.
  static void GetPublicKeyHash(std::vector<uint8_t>* hash);

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
  // After the first call, ComponentReady will be no-op for new versions
  // delivered from Component Updater, i.e. new components will be installed
  // (kept on-disk) but not propagated to the NetworkService until next
  // browser startup.
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  // We use a OnceCallback to ensure we only pass along the sets file once
  // during Chromium's lifetime.
  SetsReadyOnceCallback on_sets_ready_;

  base::TaskPriority priority_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_FIRST_PARTY_SETS_COMPONENT_INSTALLER_POLICY_H_
