// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/history_search_strings_component_installer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/history_embeddings/core/search_strings_update_listener.h"

namespace component_updater {

namespace {

// The SHA256 of the public key used to sign the extension.
// The extension id is: pkomkdjpmjfbkgjjmmaioegaojgdahkm
constexpr auto kHistorySearchStringsPublicKeySHA256 = std::to_array<uint8_t>(
    {0xfa, 0xec, 0xa3, 0x9f, 0xc9, 0x51, 0xa6, 0x99, 0xcc, 0x08, 0xe4,
     0x60, 0xe9, 0x63, 0x07, 0xac, 0xab, 0x81, 0x71, 0x26, 0x3e, 0x0f,
     0x27, 0xa8, 0x69, 0xbe, 0xf3, 0xe0, 0x7c, 0x20, 0x6c, 0x7d});

base::FilePath GetInstalledPath(const base::FilePath& install_dir) {
  return install_dir.AppendASCII(kHistorySearchStringsBinaryPbFileName);
}

}  // namespace

HistorySearchStringsComponentInstallerPolicy::
    HistorySearchStringsComponentInstallerPolicy() = default;

HistorySearchStringsComponentInstallerPolicy::
    ~HistorySearchStringsComponentInstallerPolicy() = default;

bool HistorySearchStringsComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to validate the proto file here. It will be validated in
  // history_embeddings::SearchStringsUpdateListener.
  return base::PathExists(GetInstalledPath(install_dir));
}

bool HistorySearchStringsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool HistorySearchStringsComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
HistorySearchStringsComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void HistorySearchStringsComponentInstallerPolicy::OnCustomUninstall() {}

void HistorySearchStringsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DVLOG(1) << "History Search component ready in "
           << GetInstalledPath(install_dir);
  history_embeddings::SearchStringsUpdateListener::GetInstance()
      ->OnSearchStringsUpdate(GetInstalledPath(install_dir));
}

base::FilePath
HistorySearchStringsComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("HistorySearch"));
}

void HistorySearchStringsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kHistorySearchStringsPublicKeySHA256),
               std::end(kHistorySearchStringsPublicKeySHA256));
}

std::string HistorySearchStringsComponentInstallerPolicy::GetName() const {
  return "History Search";
}

update_client::InstallerAttributes
HistorySearchStringsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterHistorySearchStringsComponent(ComponentUpdateService* cus) {
  DVLOG(1) << "Registering History Search component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<HistorySearchStringsComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

void DeleteHistorySearchStringsComponent(const base::FilePath& user_data_dir) {
  DVLOG(1) << "Deleting History Search component.";
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                     user_data_dir.Append(FILE_PATH_LITERAL("HistorySearch"))));
}

}  // namespace component_updater
