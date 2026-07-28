// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/private_verification_tokens_installer_policy.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component's id is: opbecjondbhibnecdjlbkbcidflgdebf
constexpr uint8_t kPrivateVerificationTokensPublicKeySHA256[32] = {
    0xef, 0x14, 0x29, 0xed, 0x31, 0x78, 0x1d, 0x42, 0x39, 0xb1, 0xa1,
    0x28, 0x35, 0xb6, 0x34, 0x15, 0xc4, 0xb9, 0x63, 0x36, 0xb5, 0x0b,
    0x08, 0xfe, 0xcf, 0x1a, 0xfc, 0x4e, 0x79, 0x4a, 0xe4, 0xfd};

constexpr char kPrivateVerificationTokensManifestName[] =
    "Private Verification Token";

}  // namespace

namespace component_updater {

PrivateVerificationTokensInstallerPolicy::
    PrivateVerificationTokensInstallerPolicy(OnComponentReadyCallback callback)
    : callback_(std::move(callback)) {}

PrivateVerificationTokensInstallerPolicy::
    ~PrivateVerificationTokensInstallerPolicy() = default;

bool PrivateVerificationTokensInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kPvtConfigFileName));
}

bool PrivateVerificationTokensInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PrivateVerificationTokensInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
PrivateVerificationTokensInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void PrivateVerificationTokensInstallerPolicy::OnCustomUninstall() {}

void PrivateVerificationTokensInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  base::FilePath file_path = install_dir.Append(kPvtConfigFileName);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&private_verification_tokens::
                         PrivateVerificationTokensIssuerConfig::LoadFromFile,
                     file_path),
      base::BindOnce(callback_));
}

base::FilePath PrivateVerificationTokensInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("PrivateVerificationTokens"));
}

void PrivateVerificationTokensInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetPublicKeyHash(hash);
}

std::string PrivateVerificationTokensInstallerPolicy::GetName() const {
  return kPrivateVerificationTokensManifestName;
}

update_client::InstallerAttributes
PrivateVerificationTokensInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

// static
void PrivateVerificationTokensInstallerPolicy::GetPublicKeyHash(
    std::vector<uint8_t>* hash) {
  CHECK(hash);
  hash->assign(std::begin(kPrivateVerificationTokensPublicKeySHA256),
               std::end(kPrivateVerificationTokensPublicKeySHA256));
}

}  // namespace component_updater
