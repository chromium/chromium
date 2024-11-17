// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/open_cookie_database_component_installer_policy.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The extension id is: pmagihnlncbcefglppponlgakiphldeh
constexpr uint8_t kOpenCookieDatabasePublicKeySha256[32] = {
    0xfc, 0x06, 0x87, 0xdb, 0xd2, 0x12, 0x45, 0x6b, 0xff, 0xfe, 0xdb,
    0x60, 0xa8, 0xf7, 0xb3, 0x47, 0xe0, 0x1e, 0x78, 0xf0, 0x36, 0x5f,
    0xa4, 0xe4, 0x06, 0x0e, 0x0b, 0x03, 0x89, 0x5e, 0x27, 0x63};

constexpr char kOpenCookieDatabaseManifestName[] = "Open Cookie Database";

constexpr base::FilePath::CharType kOpenCookieDatabaseCsvFileName[] =
    FILE_PATH_LITERAL("open-cookie-database.csv");

constexpr base::FilePath::CharType kOpenCookieDatabaseRelativeInstallDir[] =
    FILE_PATH_LITERAL("OpenCookieDatabase");

void LoadOpenCookieDatabaseFromDisk(const base::FilePath& csv_path) {
  if (csv_path.empty()) {
    return;
  }

  VLOG(1) << "Reading Open Cookie Database from file: " << csv_path.value();
  std::string csv_content;
  if (!base::ReadFileToString(csv_path, &csv_content)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    VLOG(1) << "Failed reading from " << csv_path.value();
    return;
  }
  // TODO(crbug.com/366423913): Implement Open Cookie Database handler and pass
  // in csv.
}

base::FilePath GetInstalledPath(const base::FilePath& path) {
  return path.Append(kOpenCookieDatabaseCsvFileName);
}

}  // namespace

namespace component_updater {

bool OpenCookieDatabaseComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool OpenCookieDatabaseComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
OpenCookieDatabaseComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void OpenCookieDatabaseComponentInstallerPolicy::OnCustomUninstall() {}

void OpenCookieDatabaseComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadOpenCookieDatabaseFromDisk,
                     GetInstalledPath(install_dir)));
}

// Called during startup and installation before ComponentReady().
bool OpenCookieDatabaseComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // TODO(crbug.com/366423913): - Actual verification will happen in the Open
  // Cookie Database handler.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
OpenCookieDatabaseComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(kOpenCookieDatabaseRelativeInstallDir);
}

void OpenCookieDatabaseComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kOpenCookieDatabasePublicKeySha256),
               std::end(kOpenCookieDatabasePublicKeySha256));
}

std::string OpenCookieDatabaseComponentInstallerPolicy::GetName() const {
  return kOpenCookieDatabaseManifestName;
}

update_client::InstallerAttributes
OpenCookieDatabaseComponentInstallerPolicy::GetInstallerAttributes() const {
  return {};
}

}  // namespace component_updater
