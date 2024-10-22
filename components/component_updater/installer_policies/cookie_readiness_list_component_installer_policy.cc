// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/cookie_readiness_list_component_installer_policy.h"

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
// The extension id is: mcfjlbnicoclaecapilmleaelokfnijm
constexpr uint8_t kCookieReadinessListPublicKeySha256[32] = {
    0xc2, 0x59, 0xb1, 0xd8, 0x2e, 0x2b, 0x04, 0x20, 0xf8, 0xbc, 0xb4,
    0x04, 0xbe, 0xa5, 0xd8, 0x9c, 0x20, 0x23, 0x74, 0x8f, 0xa7, 0x72,
    0xfa, 0xfb, 0x7d, 0x64, 0x6b, 0x76, 0x29, 0x31, 0xd2, 0xe3};

constexpr char kCookieReadinessListManifestName[] = "Cookie Readiness List";

constexpr base::FilePath::CharType kCookieReadinessListJsonFileName[] =
    FILE_PATH_LITERAL("cookie-readiness-list.json");

constexpr base::FilePath::CharType kCookieReadinessListRelativeInstallDir[] =
    FILE_PATH_LITERAL("CookieReadinessList");

void LoadCookieReadinessListFromDisk(const base::FilePath& json_path) {
  if (json_path.empty()) {
    return;
  }

  VLOG(1) << "Reading Cookie Readiness List from file: " << json_path.value();
  std::string json_content;
  if (!base::ReadFileToString(json_path, &json_content)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    VLOG(1) << "Failed reading from " << json_path.value();
    return;
  }
  // TODO(crbug.com/372881302): Implement Cookie Readiness List handler and pass
  // in json.
}

base::FilePath GetInstalledPath(const base::FilePath& path) {
  return path.Append(kCookieReadinessListJsonFileName);
}

}  // namespace

namespace component_updater {

bool CookieReadinessListComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool CookieReadinessListComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
CookieReadinessListComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void CookieReadinessListComponentInstallerPolicy::OnCustomUninstall() {}

void CookieReadinessListComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadCookieReadinessListFromDisk,
                     GetInstalledPath(install_dir)));
}

// Called during startup and installation before ComponentReady().
bool CookieReadinessListComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // TODO(crbug.com/372881302): - Actual verification will happen in the Cookie
  // Readiness List handler.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
CookieReadinessListComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(kCookieReadinessListRelativeInstallDir);
}

void CookieReadinessListComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kCookieReadinessListPublicKeySha256),
               std::end(kCookieReadinessListPublicKeySha256));
}

std::string CookieReadinessListComponentInstallerPolicy::GetName() const {
  return kCookieReadinessListManifestName;
}

update_client::InstallerAttributes
CookieReadinessListComponentInstallerPolicy::GetInstallerAttributes() const {
  return {};
}

}  // namespace component_updater
