// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include <utility>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/util.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace updater {

namespace {

// Version "0" corresponds to no installed version.
const char kNullVersion[] = "0.0.0.0";

// Returns the full path to the installation directory for the application
// identified by the |app_id|.
base::FilePath GetAppInstallDir(const std::string& app_id) {
  base::FilePath app_install_dir;
  if (GetProductDirectory(&app_install_dir)) {
    app_install_dir = app_install_dir.AppendASCII(kAppsDir);
    app_install_dir = app_install_dir.AppendASCII(app_id);
  }
  return app_install_dir;
}

}  // namespace

Installer::InstallInfo::InstallInfo() : version(kNullVersion) {}
Installer::InstallInfo::~InstallInfo() = default;

Installer::Installer(const std::string& app_id)
    : app_id_(app_id), install_info_(std::make_unique<InstallInfo>()) {}

Installer::~Installer() = default;

update_client::CrxComponent Installer::MakeCrxComponent() {
  update_client::CrxComponent component;
  component.installer = scoped_refptr<Installer>(this);
  component.requires_network_encryption = false;
  component.crx_format_requirement =
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
  component.app_id = app_id_;
  component.name = app_id_;
  component.version = install_info_->version;
  component.fingerprint = install_info_->fingerprint;
  return component;
}

std::vector<std::string> Installer::FindAppIds() {
  base::FilePath app_install_dir;
  if (!GetProductDirectory(&app_install_dir))
    return {};
  app_install_dir = app_install_dir.AppendASCII(kAppsDir);
  std::vector<std::string> app_ids;
  base::FileEnumerator file_enumerator(app_install_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (auto path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    app_ids.push_back(path.BaseName().MaybeAsASCII());
  }
  return app_ids;
}

void Installer::FindInstallOfApp() {
  VLOG(1) << __func__ << " for " << app_id_;

  const base::FilePath app_install_dir = GetAppInstallDir(app_id_);
  if (app_install_dir.empty() || !base::PathExists(app_install_dir)) {
    install_info_ = std::make_unique<InstallInfo>();
    return;
  }

  base::Version latest_version(kNullVersion);
  base::FilePath latest_path;
  std::vector<base::FilePath> older_paths;
  base::FileEnumerator file_enumerator(app_install_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (auto path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    const base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names.
    if (!version.IsValid())
      continue;

    // The |version| not newer than the latest found version is marked for
    // removal. |kNullVersion| is also removed.
    if (version.CompareTo(latest_version) <= 0) {
      older_paths.push_back(path);
      continue;
    }

    // New valid |version| folder found.
    if (!latest_path.empty())
      older_paths.push_back(latest_path);

    latest_version = version;
    latest_path = path;
  }

  install_info_->version = latest_version;
  install_info_->install_dir = latest_path;
  install_info_->manifest = update_client::ReadManifest(latest_path);
  base::ReadFileToString(latest_path.AppendASCII("manifest.fingerprint"),
                         &install_info_->fingerprint);

  for (const auto& older_path : older_paths)
    base::DeleteFile(older_path, true);
}

Installer::Result Installer::InstallHelper(const base::FilePath& unpack_path) {
  auto local_manifest = update_client::ReadManifest(unpack_path);
  if (!local_manifest)
    return Result(update_client::InstallError::BAD_MANIFEST);

  std::string version_ascii;
  local_manifest->GetStringASCII("version", &version_ascii);
  const base::Version manifest_version(version_ascii);

  VLOG(1) << "Installed version=" << install_info_->version.GetString()
          << ", installing version=" << manifest_version.GetString();

  if (!manifest_version.IsValid())
    return Result(update_client::InstallError::INVALID_VERSION);

  if (install_info_->version.CompareTo(manifest_version) > 0)
    return Result(update_client::InstallError::VERSION_NOT_UPGRADED);

  const base::FilePath app_install_dir = GetAppInstallDir(app_id_);
  if (app_install_dir.empty())
    return Result(update_client::InstallError::NO_DIR_COMPONENT_USER);
  if (!base::CreateDirectory(app_install_dir)) {
    return Result(
        static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE) +
        kCustomInstallErrorCreateAppInstallDirectory);
  }

  const auto versioned_install_dir =
      app_install_dir.AppendASCII(manifest_version.GetString());
  if (base::PathExists(versioned_install_dir)) {
    if (!base::DeleteFile(versioned_install_dir, true))
      return Result(update_client::InstallError::CLEAN_INSTALL_DIR_FAILED);
  }

  VLOG(1) << "Install_path=" << versioned_install_dir.AsUTF8Unsafe();

  if (!base::Move(unpack_path, versioned_install_dir)) {
    PLOG(ERROR) << "Move failed.";
    base::DeleteFile(versioned_install_dir, true);
    return Result(update_client::InstallError::MOVE_FILES_ERROR);
  }

  DCHECK(!base::PathExists(unpack_path));
  DCHECK(base::PathExists(versioned_install_dir));

  install_info_->manifest = std::move(local_manifest);
  install_info_->version = manifest_version;
  install_info_->install_dir = versioned_install_dir;
  base::ReadFileToString(
      versioned_install_dir.AppendASCII("manifest.fingerprint"),
      &install_info_->fingerprint);

  return Result(update_client::InstallError::NONE);
}

void Installer::OnUpdateError(int error) {
  LOG(ERROR) << "updater error: " << error << " for " << app_id_;
}

void Installer::Install(const base::FilePath& unpack_path,
                        const std::string& public_key,
                        Callback callback) {
  std::unique_ptr<base::DictionaryValue> manifest;
  base::Version version;
  base::FilePath install_path;

  const auto result = InstallHelper(unpack_path);
  base::DeleteFile(unpack_path, true);
  std::move(callback).Run(result);
}

bool Installer::GetInstalledFile(const std::string& file,
                                 base::FilePath* installed_file) {
  return false;
}

bool Installer::Uninstall() {
  return false;
}

}  // namespace updater
