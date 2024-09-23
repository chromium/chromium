// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_POLICY_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_POLICY_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"

namespace base {
class FilePath;
}  // namespace base

inline constexpr base::FilePath::CharType kMaskedDomainListFileName[] =
    FILE_PATH_LITERAL("list.pb");

namespace component_updater {

class ComponentUpdateService;

class MaskedDomainListComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  using ListReadyRepeatingCallback =
      base::RepeatingCallback<void(base::Version,
                                   std::optional<mojo_base::ProtoWrapper>)>;

  // |on_list_ready| will be called on the UI thread when the list is ready. It
  // is exposed here for testing.
  explicit MaskedDomainListComponentInstallerPolicy(
      ListReadyRepeatingCallback on_list_ready);
  ~MaskedDomainListComponentInstallerPolicy() override;

  MaskedDomainListComponentInstallerPolicy(
      const MaskedDomainListComponentInstallerPolicy&) = delete;
  MaskedDomainListComponentInstallerPolicy operator=(
      const MaskedDomainListComponentInstallerPolicy&) = delete;

  static bool IsEnabled();

  // Returns the component's SHA2 hash as raw bytes.
  static void GetPublicKeyHash(std::vector<uint8_t>* hash);

  static base::FilePath GetInstalledPath(const base::FilePath& base);

 private:
  FRIEND_TEST_ALL_PREFIXES(MaskedDomainListComponentInstallerPolicyTest,
                           NonexistentFile);
  FRIEND_TEST_ALL_PREFIXES(MaskedDomainListComponentInstallerPolicyTest,
                           NonexistentFile_OnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(MaskedDomainListComponentInstallerPolicyTest,
                           LoadsFile_OnComponentReady);
  FRIEND_TEST_ALL_PREFIXES(MaskedDomainListComponentInstallerPolicyTest,
                           LoadsNewListWhenUpdated);

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

  ListReadyRepeatingCallback on_list_ready_;
};

// Call once during startup to make the component update service aware of
// the Masked Domain List component.
void RegisterMaskedDomainListComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_MASKED_DOMAIN_LIST_COMPONENT_INSTALLER_POLICY_H_
