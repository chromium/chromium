// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/component_updater/installer_policies/masked_domain_list_component_installer_policy.h"

#include <optional>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "services/network/public/cpp/features.h"

using component_updater::ComponentUpdateService;

namespace {

using ListReadyRepeatingCallback = component_updater::
    MaskedDomainListComponentInstallerPolicy::ListReadyRepeatingCallback;

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: cffplpkejcbdpfnfabnjikeicbedmifn
constexpr uint8_t kMaskedDomainListPublicKeySHA256[32] = {
    0x25, 0x5f, 0xbf, 0xa4, 0x92, 0x13, 0xf5, 0xd5, 0x01, 0xd9, 0x8a,
    0x48, 0x21, 0x43, 0xc8, 0x5d, 0x5c, 0x49, 0x4f, 0xdc, 0xa9, 0x31,
    0xba, 0x61, 0x1d, 0x2e, 0xe0, 0x0e, 0x26, 0x76, 0x03, 0xf9};

constexpr char kMaskedDomainListManifestName[] = "Masked Domain List";

constexpr char kExperimentalVersionAttributeName[] =
    "_experimental_mdl_version";

constexpr base::FilePath::CharType kMaskedDomainListRelativeInstallDir[] =
    FILE_PATH_LITERAL("MaskedDomainListPreloaded");

constexpr char kMaskedDomainListProto[] = "masked_domain_list.MaskedDomainList";

}  // namespace

namespace component_updater {

// Helper class to read file to named proto class for mojo wrapper.
class ReadMaskedDomainListProto {
 public:
  static std::optional<mojo_base::ProtoWrapper> ReadFile(
      const base::FilePath& pb_path) {
    auto file_contents = base::ReadFileToBytes(pb_path);
    if (file_contents.has_value()) {
      return mojo_base::ProtoWrapper(
          file_contents.value(), kMaskedDomainListProto,
          mojo_base::ProtoWrapperBytes::GetPassKey());
    }
    return std::nullopt;
  }
};

MaskedDomainListComponentInstallerPolicy::
    MaskedDomainListComponentInstallerPolicy(
        ListReadyRepeatingCallback on_list_ready)
    : on_list_ready_(on_list_ready) {
  CHECK(on_list_ready);
}

MaskedDomainListComponentInstallerPolicy::
    ~MaskedDomainListComponentInstallerPolicy() = default;

bool MaskedDomainListComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

// static
bool MaskedDomainListComponentInstallerPolicy::IsEnabled() {
  return base::FeatureList::IsEnabled(network::features::kMaskedDomainList);
}

bool MaskedDomainListComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  // Update checks and pings associated with this component do not require
  // confidentiality, since the component is identical for all users.
  return false;
}

update_client::CrxInstaller::Result
MaskedDomainListComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void MaskedDomainListComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath MaskedDomainListComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kMaskedDomainListFileName);
}

void MaskedDomainListComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  if (install_dir.empty() || !IsEnabled()) {
    return;
  }

  VLOG(1) << "Masked Domain List Component ready, version "
          << version.GetString() << " in " << install_dir.value();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadMaskedDomainListProto::ReadFile,
                     GetInstalledPath(install_dir)),
      base::BindOnce(on_list_ready_, version));
}

// Called during startup and installation before ComponentReady().
bool MaskedDomainListComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // Actual validation will be done in the Network Service.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath MaskedDomainListComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kMaskedDomainListRelativeInstallDir);
}

void MaskedDomainListComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetPublicKeyHash(hash);
}

// static
void MaskedDomainListComponentInstallerPolicy::GetPublicKeyHash(
    std::vector<uint8_t>* hash) {
  hash->assign(kMaskedDomainListPublicKeySHA256,
               kMaskedDomainListPublicKeySHA256 +
                   std::size(kMaskedDomainListPublicKeySHA256));
}

std::string MaskedDomainListComponentInstallerPolicy::GetName() const {
  return kMaskedDomainListManifestName;
}

update_client::InstallerAttributes
MaskedDomainListComponentInstallerPolicy::GetInstallerAttributes() const {
  return {
      {
          kExperimentalVersionAttributeName,
          network::features::kMaskedDomainListExperimentalVersion.Get(),
      },
  };
}

}  // namespace component_updater
