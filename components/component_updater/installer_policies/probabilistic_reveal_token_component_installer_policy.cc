// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/probabilistic_reveal_token_component_installer_policy.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"

namespace {
using RegistryReadyRepeatingCallback =
    component_updater::ProbabilisticRevealTokenComponentInstallerPolicy::
        RegistryReadyRepeatingCallback;

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The extension id is: ldfkbgjbencjpgjfleiooeldhjdapggh
constexpr uint8_t kProbabilisticRevealTokenPublicKeySha256[32] = {
    0xb3, 0x5a, 0x16, 0x91, 0x4d, 0x29, 0xf6, 0x95, 0xb4, 0x8e, 0xe4,
    0xb3, 0x79, 0x30, 0xf6, 0x67, 0xe1, 0x08, 0xb3, 0x65, 0x42, 0xff,
    0xa5, 0x63, 0x4e, 0xbd, 0x44, 0xbc, 0x25, 0xb2, 0xae, 0x77};

constexpr char kProbabilisticRevealTokenManifestName[] =
    "Probabilistic Reveal Tokens";

constexpr base::FilePath::CharType
    kProbabilisticRevealTokenRegistryJsonFileName[] =
        FILE_PATH_LITERAL("prt_domains.json");

constexpr base::FilePath::CharType
    kProbabilisticRevealTokenRelativeInstallDir[] =
        FILE_PATH_LITERAL("ProbabilisticRevealTokenRegistry");

std::optional<std::string> LoadProbabilisticRevealTokenRegistryFromDisk(
    const base::FilePath& json_path) {
  CHECK(!json_path.empty());

  VLOG(1) << "Reading Probabilistic Reveal Token Registry from file: "
          << json_path.value();
  std::string json_content;
  if (!base::ReadFileToString(json_path, &json_content)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    VLOG(1) << "Failed reading from " << json_path.value();
    return std::nullopt;
  }
  return json_content;
}

base::FilePath GetInstalledPath(const base::FilePath& install_dir) {
  return install_dir.Append(kProbabilisticRevealTokenRegistryJsonFileName);
}

}  // namespace

namespace component_updater {

ProbabilisticRevealTokenComponentInstallerPolicy::
    ProbabilisticRevealTokenComponentInstallerPolicy(
        RegistryReadyRepeatingCallback on_registry_ready)
    : on_registry_ready_(std::move(on_registry_ready)) {}

ProbabilisticRevealTokenComponentInstallerPolicy::
    ProbabilisticRevealTokenComponentInstallerPolicy() = default;

ProbabilisticRevealTokenComponentInstallerPolicy::
    ~ProbabilisticRevealTokenComponentInstallerPolicy() = default;

bool ProbabilisticRevealTokenComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool ProbabilisticRevealTokenComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
ProbabilisticRevealTokenComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void ProbabilisticRevealTokenComponentInstallerPolicy::OnCustomUninstall() {}

void ProbabilisticRevealTokenComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadProbabilisticRevealTokenRegistryFromDisk,
                     GetInstalledPath(install_dir)),
      base::BindOnce(on_registry_ready_));
}

// Called during startup and installation before ComponentReady().
bool ProbabilisticRevealTokenComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
ProbabilisticRevealTokenComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kProbabilisticRevealTokenRelativeInstallDir);
}

void ProbabilisticRevealTokenComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kProbabilisticRevealTokenPublicKeySha256),
               std::end(kProbabilisticRevealTokenPublicKeySha256));
}

std::string ProbabilisticRevealTokenComponentInstallerPolicy::GetName() const {
  return kProbabilisticRevealTokenManifestName;
}

update_client::InstallerAttributes
ProbabilisticRevealTokenComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return {};
}

// static
base::FilePath
ProbabilisticRevealTokenComponentInstallerPolicy::GetInstalledPathForTesting(
    const base::FilePath& base) {
  return GetInstalledPath(base);
}

}  // namespace component_updater
