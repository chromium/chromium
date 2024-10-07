// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/activity_impl_util_posix.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/linux/systemd_util.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/linux_util.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater::test {
namespace {
base::FilePath GetExecutablePath() {
  base::FilePath out_dir;
  if (!base::PathService::Get(base::DIR_EXE, &out_dir)) {
    return base::FilePath();
  }
  return out_dir.Append(GetExecutableRelativePath());
}
}  // namespace

std::optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  return GetVersionedInstallDirectory(scope, version);
}

base::FilePath GetSetupExecutablePath() {
  // There is no metainstaller on Linux, use the main executable for setup.
  return GetExecutablePath();
}

std::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetVersionedInstallDirectory(scope);
  if (!path) {
    return std::nullopt;
  }
  return path->Append(GetExecutableRelativePath());
}

bool WaitForUpdaterExit() {
  const std::set<base::FilePath::StringType> process_names =
      GetTestProcessNames();
  return WaitFor(
      [&] {
        return base::ranges::none_of(process_names,
                                     [](const auto& process_name) {
                                       return IsProcessRunning(process_name);
                                     });
      },
      [] { VLOG(0) << "Still waiting for updater to exit..."; });
}

void Uninstall(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetExecutablePath();
  ASSERT_TRUE(path);
  base::CommandLine command_line(*path);
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  EXPECT_EQ(exit_code, 0);
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetVersionedInstallDirectory(scope);
  ASSERT_TRUE(path);
  ASSERT_TRUE(WaitFor(
      [&] { return !base::PathExists(*path); },
      [] { VLOG(0) << "Waiting for the candidate to be uninstalled."; }));
}

void ExpectInstalled(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetInstalledExecutablePath(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(base::PathExists(*path));
  }
}

void Clean(UpdaterScope scope) {
  std::optional<base::FilePath> path = GetInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  }

  EXPECT_TRUE(UninstallSystemdUnits(scope));

  if (IsSystemInstall(scope)) {
    ASSERT_NO_FATAL_FAILURE(UninstallEnterpriseCompanionApp());
  }
}

// The uninstaller cannot reliably completely remove the installer directory
// itself, because it uses the prefs file and writes the log file while it
// is operating. If the provided path exists, it must be a directory with
// only these residual files present to be considered "clean".
void ExpectMostlyClean(const std::optional<base::FilePath>& path) {
  EXPECT_TRUE(path);
  if (!path || !base::PathExists(*path)) {
    return;
  }

  // If the path exists, expect only the log and prefs files to be present.
  int count = CountDirectoryFiles(*path);
  EXPECT_LE(count, 2);
  if (count >= 1) {
    EXPECT_TRUE(base::PathExists(path->AppendASCII("updater.log")));
  }
  if (count == 2) {
    EXPECT_TRUE(base::PathExists(path->AppendASCII("prefs.json")));
  }
}

void ExpectClean(UpdaterScope scope) {
  ExpectCleanProcesses();
  ExpectMostlyClean(GetInstallDirectory(scope));
  ExpectMostlyClean(GetCacheBaseDirectory(scope));
  EXPECT_FALSE(SystemdUnitsInstalled(scope));
  ASSERT_NO_FATAL_FAILURE(ExpectEnterpriseCompanionAppNotInstalled());
}

base::TimeDelta GetOverinstallTimeoutForEnterTestMode() {
  return TestTimeouts::action_timeout();
}

void SetActive(UpdaterScope scope, const std::string& app_id) {
  const std::optional<base::FilePath> path =
      GetActiveFile(base::GetHomeDir(), app_id);
  ASSERT_TRUE(path);
  base::File::Error err = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path->DirName(), &err))
      << "Error: " << err;
  EXPECT_TRUE(base::WriteFile(*path, ""));
}

void ExpectActive(UpdaterScope scope, const std::string& app_id) {
  const std::optional<base::FilePath> path =
      GetActiveFile(base::GetHomeDir(), app_id);
  ASSERT_TRUE(path);
  EXPECT_TRUE(base::PathExists(*path));
  EXPECT_TRUE(base::PathIsWritable(*path));
}

void ExpectNotActive(UpdaterScope scope, const std::string& app_id) {
  const std::optional<base::FilePath> path =
      GetActiveFile(base::GetHomeDir(), app_id);
  ASSERT_TRUE(path);
  EXPECT_FALSE(base::PathExists(*path));
  EXPECT_FALSE(base::PathIsWritable(*path));
}

base::FilePath GetRealUpdaterLowerVersionPath() {
  base::FilePath exe_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath old_updater_path =
      exe_path.Append(FILE_PATH_LITERAL("old_updater"));

#if BUILDFLAG(CHROMIUM_BRANDING)
  old_updater_path = old_updater_path.AppendASCII("chromium_linux64");
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
  old_updater_path = old_updater_path.AppendASCII("chrome_linux64");
#endif
#if BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
  old_updater_path = old_updater_path.AppendASCII("cipd");
#endif
  return old_updater_path.AppendASCII(
      base::StrCat({kExecutableName, kExecutableSuffix}));
}

void SetupFakeLegacyUpdater(UpdaterScope scope) {
  // No legacy migration for Linux.
}

void ExpectLegacyUpdaterMigrated(UpdaterScope scope) {
  // No legacy migration for Linux.
}

void InstallApp(UpdaterScope scope,
                const std::string& app_id,
                const base::Version& version) {
  RegistrationRequest registration;
  registration.app_id = app_id;
  registration.version = version;
  RegisterApp(scope, registration);
}

void UninstallApp(UpdaterScope scope, const std::string& app_id) {
  // This can probably be combined with mac into integration_tests_posix.cc.
  const base::FilePath& install_path =
      base::MakeRefCounted<PersistedData>(
          scope, CreateGlobalPrefs(scope)->GetPrefService(), nullptr)
          ->GetExistenceCheckerPath(app_id);
  VLOG(1) << "Deleting app install path: " << install_path;
  base::DeletePathRecursively(install_path);
  SetExistenceCheckerPath(scope, app_id,
                          base::FilePath(FILE_PATH_LITERAL("NONE")));
}

base::CommandLine MakeElevated(base::CommandLine command_line) {
  command_line.PrependWrapper("/usr/bin/sudo");
  return command_line;
}

void SetPlatformPolicies(const base::Value::Dict& values) {}

void ExpectAppVersion(UpdaterScope scope,
                      const std::string& app_id,
                      const base::Version& version) {
  const base::Version app_version =
      base::MakeRefCounted<PersistedData>(
          scope, CreateGlobalPrefs(scope)->GetPrefService(), nullptr)
          ->GetProductVersion(app_id);
  EXPECT_TRUE(app_version.IsValid());
  EXPECT_EQ(version, app_version);
}

}  // namespace updater::test
