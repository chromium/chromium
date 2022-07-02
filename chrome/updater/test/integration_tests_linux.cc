// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// TODO(crbug.com/1276180) - implement these functions.

namespace updater {
namespace test {

absl::optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  NOTREACHED();
  return absl::nullopt;
}

base::FilePath GetSetupExecutablePath() {
  NOTREACHED();
  return base::FilePath();
}

absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  NOTREACHED();
  return absl::nullopt;
}

void WaitForUpdaterExit(UpdaterScope scope) {
  NOTREACHED();
}

absl::optional<base::FilePath> GetDataDirPath(UpdaterScope scope) {
  NOTREACHED();
  return absl::nullopt;
}

void Uninstall(UpdaterScope scope) {
  NOTREACHED();
}

void ExpectActiveUpdater(UpdaterScope scope) {
  NOTREACHED();
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  NOTREACHED();
}

void ExpectInstalled(UpdaterScope scope) {
  NOTREACHED();
}

void Clean(UpdaterScope scope) {
  NOTREACHED();
}

void ExpectClean(UpdaterScope scope) {
  NOTREACHED();
}

void EnterTestMode(const GURL& url) {
  NOTREACHED();
}

void SetActive(UpdaterScope scope, const std::string& app_id) {
  NOTREACHED();
}

void ExpectActive(UpdaterScope scope, const std::string& app_id) {
  NOTREACHED();
}

void ExpectNotActive(UpdaterScope scope, const std::string& app_id) {
  NOTREACHED();
}

void SetupRealUpdaterLowerVersion(UpdaterScope scope) {
  NOTREACHED();
}

void SetupFakeLegacyUpdaterData(UpdaterScope scope) {
  NOTREACHED();
}

void ExpectLegacyUpdaterDataMigrated(UpdaterScope scope) {
  NOTREACHED();
}

void InstallApp(UpdaterScope scope, const std::string& app_id) {
  NOTREACHED();
}

void UninstallApp(UpdaterScope scope, const std::string& app_id) {
  // This can probably be combined with mac into integration_tests_posix.cc.
  NOTREACHED();
}

void RunOfflineInstall(UpdaterScope scope) {
  NOTREACHED();
}

}  // namespace test
}  // namespace updater
