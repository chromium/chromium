// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_switches.h"

using component_updater::ComponentUpdateService;

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: kiabhabjdbkjdpjbpigfodbdjmbglcoo
const uint8_t kTrustTokenKeyCommitmentsPublicKeySHA256[32] = {
    0xa8, 0x01, 0x70, 0x19, 0x31, 0xa9, 0x3f, 0x91, 0xf8, 0x65, 0xe3,
    0x13, 0x9c, 0x16, 0xb2, 0xee, 0xb4, 0xc7, 0xc2, 0x8e, 0xdb, 0x04,
    0xd3, 0xaf, 0xeb, 0x07, 0x18, 0x15, 0x89, 0x23, 0x81, 0xad};

const char kTrustTokenKeyCommitmentsManifestName[] =
    "Trust Token Key Commitments";

// Attempts to load key commitments as raw JSON from their storage file,
// returning the loaded commitments on success and nullopt on failure.
std::optional<std::string> LoadKeyCommitmentsFromDisk(
    const base::FilePath& path) {
  if (path.empty()) {
    return std::nullopt;
  }

  VLOG(1) << "Reading trust token key commitments from file: " << path.value();

  std::string ret;
  if (!base::ReadFileToString(path, &ret)) {
    VLOG(1) << "Failed reading from " << path.value();
    return std::nullopt;
  }

  return ret;
}

}  // namespace

namespace component_updater {

TrustTokenKeyCommitmentsComponentInstallerPolicy::
    TrustTokenKeyCommitmentsComponentInstallerPolicy(
        base::RepeatingCallback<void(const std::string&)> on_commitments_ready)
    : on_commitments_ready_(std::move(on_commitments_ready)) {}

TrustTokenKeyCommitmentsComponentInstallerPolicy::
    ~TrustTokenKeyCommitmentsComponentInstallerPolicy() = default;

bool TrustTokenKeyCommitmentsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool TrustTokenKeyCommitmentsComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  // A network-path adversary being able to change the key commitments would
  // nullify the Trust Tokens protocol's privacy properties---but the component
  // updater guarantees integrity even if we return false here, and we don't
  // need confidentiality since this component's value is public and identical
  // for all users.
  return false;
}

update_client::CrxInstaller::Result
TrustTokenKeyCommitmentsComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void TrustTokenKeyCommitmentsComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath
TrustTokenKeyCommitmentsComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kComponentUpdaterTrustTokensComponentPath)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        switches::kComponentUpdaterTrustTokensComponentPath);
  }

  return base.Append(kTrustTokenKeyCommitmentsFileName);
}

void TrustTokenKeyCommitmentsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  LoadTrustTokensFromString(base::BindOnce(&LoadKeyCommitmentsFromDisk,
                                           GetInstalledPath(install_dir)),
                            on_commitments_ready_);
}

// Called during startup and installation before ComponentReady().
bool TrustTokenKeyCommitmentsComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the commitments here, since we'll do the
  // checking in NetworkService::SetTrustTokenKeyCommitments.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
TrustTokenKeyCommitmentsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("TrustTokenKeyCommitments"));
}

void TrustTokenKeyCommitmentsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetPublicKeyHash(hash);
}

std::string TrustTokenKeyCommitmentsComponentInstallerPolicy::GetName() const {
  return kTrustTokenKeyCommitmentsManifestName;
}

update_client::InstallerAttributes
TrustTokenKeyCommitmentsComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

// static
void TrustTokenKeyCommitmentsComponentInstallerPolicy::GetPublicKeyHash(
    std::vector<uint8_t>* hash) {
  DCHECK(hash);
  hash->assign(kTrustTokenKeyCommitmentsPublicKeySHA256,
               kTrustTokenKeyCommitmentsPublicKeySHA256 +
                   std::size(kTrustTokenKeyCommitmentsPublicKeySHA256));
}

// static
void TrustTokenKeyCommitmentsComponentInstallerPolicy::
    LoadTrustTokensFromString(
        base::OnceCallback<std::optional<std::string>()> load_keys_from_disk,
        base::OnceCallback<void(const std::string&)> on_commitments_ready) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      std::move(load_keys_from_disk),
      base::BindOnce(
          // Only bother sending commitments to the network service if we loaded
          // them successfully.
          [](base::OnceCallback<void(const std::string&)> on_commitments_ready,
             std::optional<std::string> loaded_commitments) {
            if (loaded_commitments.has_value()) {
              std::move(on_commitments_ready).Run(loaded_commitments.value());
            }
          },
          std::move(on_commitments_ready)));
}

}  // namespace component_updater
