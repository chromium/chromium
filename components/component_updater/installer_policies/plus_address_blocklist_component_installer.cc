// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/plus_address_blocklist_component_installer.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_blocklist_data.h"

namespace component_updater {
namespace {

const base::FilePath::CharType kPlusAddressBlocklistBinaryPbFileName[] =
    FILE_PATH_LITERAL("compact_blocked_facets.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: mcfcpknbdmljknapnofahjlmhmddmfkb
constexpr std::array<uint8_t, 32> kPlusAddressBlocklistPublicKeySHA256 = {
    0xc2, 0x52, 0xfa, 0xd1, 0x3c, 0xb9, 0xad, 0x0f, 0xde, 0x50, 0x79,
    0xbc, 0x7c, 0x33, 0xc5, 0xa1, 0x3f, 0x30, 0x0e, 0xb5, 0xd8, 0xe5,
    0x49, 0x78, 0xa4, 0x33, 0x90, 0xa6, 0xa9, 0x3d, 0x72, 0xfe};

base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(kPlusAddressBlocklistBinaryPbFileName);
}

std::optional<std::string> LoadPlusAddressBlocklistFromDisk(
    const base::FilePath& pb_path) {
  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    DVLOG(1) << "Failed reading from " << pb_path.value();
    return std::nullopt;
  }

  return binary_pb;
}

void PopulatePlusAddressBlocklistData(const base::FilePath& pb_path,
                                      std::optional<std::string> binary_pb) {
  if (!binary_pb) {
    return;
  }
  bool parsing_result = plus_addresses::PlusAddressBlocklistData::GetInstance()
                            .PopulateDataFromComponent(std::move(*binary_pb));
  if (!parsing_result) {
    DVLOG(1) << "Failed parsing proto " << pb_path.value();
    return;
  }
}

}  // namespace

PlusAddressBlocklistInstallerPolicy::PlusAddressBlocklistInstallerPolicy() =
    default;

PlusAddressBlocklistInstallerPolicy::~PlusAddressBlocklistInstallerPolicy() =
    default;

bool PlusAddressBlocklistInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PlusAddressBlocklistInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
PlusAddressBlocklistInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PlusAddressBlocklistInstallerPolicy::OnCustomUninstall() {}

void PlusAddressBlocklistInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict /* manifest */) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
  const base::FilePath pb_path = GetInstalledPath(install_dir);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadPlusAddressBlocklistFromDisk, pb_path),
      base::BindOnce(&PopulatePlusAddressBlocklistData, pb_path));
}

// Called during startup and installation before ComponentReady().
bool PlusAddressBlocklistInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath PlusAddressBlocklistInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("PlusAddressBlocklist"));
}

void PlusAddressBlocklistInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPlusAddressBlocklistPublicKeySHA256),
               std::end(kPlusAddressBlocklistPublicKeySHA256));
}

std::string PlusAddressBlocklistInstallerPolicy::GetName() const {
  return "Plus Address Blocklist";
}

update_client::InstallerAttributes
PlusAddressBlocklistInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterPlusAddressBlocklistComponent(ComponentUpdateService* cus) {
  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressBlocklistEnabled)) {
    DVLOG(1) << "Registering Plus Address Blocklist component.";

    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<PlusAddressBlocklistInstallerPolicy>());
    installer->Register(cus, base::OnceClosure());
  }
}

}  // namespace component_updater
