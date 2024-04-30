// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/tpcd_metadata_component_installer_policy.h"

#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "net/base/features.h"

using component_updater::ComponentUpdateService;

namespace {
// This is similar to the display name at http://omaharelease/1915488/settings
// and
// http://google3/java/com/google/installer/releasemanager/Automation.java;l=1161;rcl=553816031
constexpr char kTpcdMetadataManifestName[] =
    "Third-Party Cookie Deprecation Metadata";

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: jflhchccmppkfebkiaminageehmchikm
constexpr uint8_t kTpcdMetadataPublicKeySHA256[32] = {
    0x95, 0xb7, 0x27, 0x22, 0xcf, 0xfa, 0x54, 0x1a, 0x80, 0xc8, 0xd0,
    0x64, 0x47, 0xc2, 0x78, 0xac, 0x61, 0x26, 0x43, 0xbf, 0x3a, 0x51,
    0x2e, 0xa6, 0xce, 0x00, 0x25, 0x7b, 0x6c, 0xc4, 0x4e, 0x39};

constexpr base::FilePath::CharType kRelInstallDirName[] =
    FILE_PATH_LITERAL("TpcdMetadata");

// Runs on a thread pool.
std::optional<std::string> ReadComponentFromDisk(
    const base::FilePath& file_path) {
  VLOG(1) << "Reading TPCD Metadata from file: " << file_path.value();
  std::string contents;
  if (!base::ReadFileToString(file_path, &contents)) {
    VLOG(1) << "Failed reading from " << file_path.value();
    return std::nullopt;
  }
  return contents;
}

base::FilePath GetComponentPath(const base::FilePath& install_dir) {
  return install_dir.Append(component_updater::kTpcdMetadataComponentFileName);
}
}  // namespace

namespace component_updater {
TpcdMetadataComponentInstallerPolicy::TpcdMetadataComponentInstallerPolicy(
    OnTpcdMetadataComponentReadyCallback on_component_ready_callback)
    : on_component_ready_callback_(on_component_ready_callback) {}

TpcdMetadataComponentInstallerPolicy::~TpcdMetadataComponentInstallerPolicy() =
    default;

// Start of ComponentInstallerPolicy overrides impl:
bool TpcdMetadataComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool TpcdMetadataComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
TpcdMetadataComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void TpcdMetadataComponentInstallerPolicy::OnCustomUninstall() {}

void TpcdMetadataComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "TPCD Metadata Component ready, version " << version.GetString()
          << " in " << install_dir.value();

  if (base::FeatureList::IsEnabled(net::features::kTpcdMetadataGrants)) {
    // Given `BEST_EFFORT` since we don't need to be USER_BLOCKING.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ReadComponentFromDisk, GetComponentPath(install_dir)),
        base::BindOnce(
            [](OnTpcdMetadataComponentReadyCallback on_component_ready_callback,
               const std::optional<std::string>& maybe_contents) {
              if (maybe_contents.has_value()) {
                on_component_ready_callback.Run(maybe_contents.value());
              }
            },
            on_component_ready_callback_));
  }
}

void WriteMetrics(tpcd::metadata::InstallationResult result) {
  base::UmaHistogramEnumeration(
      "Navigation.TpcdMitigations.MetadataInstallationResult", result);
}

// Called during startup and installation before ComponentReady().
bool TpcdMetadataComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(GetComponentPath(install_dir))) {
    WriteMetrics(tpcd::metadata::InstallationResult::kMissingMetadataFile);
    return false;
  }

  std::string contents;
  if (!base::ReadFileToString(GetComponentPath(install_dir), &contents)) {
    WriteMetrics(
        tpcd::metadata::InstallationResult::kReadingMetadataFileFailed);
    return false;
  }

  tpcd::metadata::Metadata metadata;
  if (!metadata.ParseFromString(contents)) {
    WriteMetrics(tpcd::metadata::InstallationResult::kParsingToProtoFailed);
    return false;
  }

  if (!tpcd::metadata::Parser::IsValidMetadata(metadata,
                                               base::BindOnce(WriteMetrics))) {
    return false;
  }

  WriteMetrics(tpcd::metadata::InstallationResult::kSuccessful);
  return true;
}

base::FilePath TpcdMetadataComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kRelInstallDirName);
}

// static
void TpcdMetadataComponentInstallerPolicy::GetPublicKeyHash(
    std::vector<uint8_t>* hash) {
  hash->assign(std::begin(kTpcdMetadataPublicKeySHA256),
               std::end(kTpcdMetadataPublicKeySHA256));
}

void TpcdMetadataComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetPublicKeyHash(hash);
}

std::string TpcdMetadataComponentInstallerPolicy::GetName() const {
  return kTpcdMetadataManifestName;
}

update_client::InstallerAttributes
TpcdMetadataComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}
// End of ComponentInstallerPolicy overrides impl.

}  // namespace component_updater
