// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/cookie_readiness_list_component_installer_policy.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"

namespace {
using ListReadyRepeatingCallback = component_updater::
    CookieReadinessListComponentInstallerPolicy::ListReadyRepeatingCallback;

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

std::optional<std::string> LoadCookieReadinessListFromDisk(
    const base::FilePath& json_path) {
  CHECK(!json_path.empty());

  VLOG(1) << "Reading Cookie Readiness List from file: " << json_path.value();
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
  return install_dir.Append(kCookieReadinessListJsonFileName);
}

}  // namespace

namespace component_updater {

CookieReadinessListComponentInstallerPolicy::
    CookieReadinessListComponentInstallerPolicy(
        ListReadyRepeatingCallback on_list_ready)
    : on_list_ready_(std::move(on_list_ready)) {}

CookieReadinessListComponentInstallerPolicy::
    CookieReadinessListComponentInstallerPolicy() = default;

CookieReadinessListComponentInstallerPolicy::
    ~CookieReadinessListComponentInstallerPolicy() = default;

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

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadCookieReadinessListFromDisk,
                     GetInstalledPath(install_dir)),
      base::BindOnce(on_list_ready_));
}

// Called during startup and installation before ComponentReady().
bool CookieReadinessListComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
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

// static
base::FilePath
CookieReadinessListComponentInstallerPolicy::GetInstalledPathForTesting(
    const base::FilePath& base) {
  return GetInstalledPath(base);
}

}  // namespace component_updater
