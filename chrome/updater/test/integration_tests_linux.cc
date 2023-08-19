// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "build/branding_buildflags.h"
#include "chrome/updater/activity_impl_util_posix.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/linux/systemd_util.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/linux_util.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

absl::optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  return GetVersionedInstallDirectory(scope, version);
}

base::FilePath GetSetupExecutablePath() {
  // There is no metainstaller on Linux, use the main executable for setup.
  return GetExecutablePath();
}

absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetVersionedInstallDirectory(scope);
  if (!path) {
    return absl::nullopt;
  }
  return path->Append(GetExecutableRelativePath());
}

bool WaitForUpdaterExit(UpdaterScope /*scope*/) {
  const std::set<base::FilePath::StringType> process_names =
      GetTestProcessNames();
  return WaitFor(
      [&process_names]() {
        return base::ranges::none_of(process_names, IsProcessRunning);
      },
      [] { VLOG(0) << "Still waiting for updater to exit..."; });
}

void Uninstall(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetExecutablePath();
  ASSERT_TRUE(path);
  base::CommandLine command_line(*path);
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  EXPECT_EQ(exit_code, 0);
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetVersionedInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_FALSE(base::PathExists(*path));
  }
}

void ExpectInstalled(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetInstalledExecutablePath(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(base::PathExists(*path));
  }
}

void Clean(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path) {
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  }

  EXPECT_TRUE(UninstallSystemdUnits(scope));
}

void ExpectClean(UpdaterScope scope) {
  ExpectCleanProcesses();

  absl::optional<base::FilePath> path = GetInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path && base::PathExists(*path)) {
    // If the path exists, then expect only the log file to be present.
    int count = CountDirectoryFiles(*path);
    EXPECT_LE(count, 2);
    if (count >= 1) {
      EXPECT_TRUE(base::PathExists(path->AppendASCII("updater.log")));
    }
    if (count == 2) {
      EXPECT_TRUE(base::PathExists(path->AppendASCII("prefs.json")));
    }
  }

  EXPECT_FALSE(SystemdUnitsInstalled(scope));
}

void EnterTestMode(const GURL& update_url,
                   const GURL& crash_upload_url,
                   const GURL& device_management_url,
                   const base::TimeDelta& idle_timeout) {
  ASSERT_TRUE(ExternalConstantsBuilder()
                  .SetUpdateURL({update_url.spec()})
                  .SetCrashUploadURL(crash_upload_url.spec())
                  .SetDeviceManagementURL(device_management_url.spec())
                  .SetUseCUP(false)
                  .SetInitialDelay(base::Milliseconds(100))
                  .SetServerKeepAliveTime(base::Seconds(2))
                  .SetCrxVerifierFormat(crx_file::VerifierFormat::CRX3)
                  .SetOverinstallTimeout(TestTimeouts::action_timeout())
                  .SetIdleCheckPeriod(idle_timeout)
                  .Modify());
}

void SetActive(UpdaterScope scope, const std::string& app_id) {
  const absl::optional<base::FilePath> path =
      GetActiveFile(base::GetHomeDir(), app_id);
  ASSERT_TRUE(path);
  base::File::Error err = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path->DirName(), &err))
      << "Error: " << err;
  EXPECT_TRUE(base::WriteFile(*path, ""));
}

void ExpectActive(UpdaterScope scope, const std::string& app_id) {
  const absl::optional<base::FilePath> path =
      GetActiveFile(base::GetHomeDir(), app_id);
  ASSERT_TRUE(path);
  EXPECT_TRUE(base::PathExists(*path));
  EXPECT_TRUE(base::PathIsWritable(*path));
}

void ExpectNotActive(UpdaterScope scope, const std::string& app_id) {
  const absl::optional<base::FilePath> path =
      GetActiveFile(base::GetHomeDir(), app_id);
  ASSERT_TRUE(path);
  EXPECT_FALSE(base::PathExists(*path));
  EXPECT_FALSE(base::PathIsWritable(*path));
}

void SetupRealUpdaterLowerVersion(UpdaterScope scope) {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  base::FilePath old_updater_path = exe_path.AppendASCII("old_updater");
#if BUILDFLAG(CHROMIUM_BRANDING)
  old_updater_path = old_updater_path.AppendASCII("chromium_linux64");
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
  old_updater_path = old_updater_path.AppendASCII("chrome_linux64");
#endif
  old_updater_path = old_updater_path.AppendASCII(
      base::StrCat({kExecutableName, kExecutableSuffix}));

  base::CommandLine command_line(old_updater_path);
  command_line.AppendSwitch(kInstallSwitch);
  LOG(ERROR) << "Command " << command_line.GetCommandLineString();
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  ASSERT_EQ(exit_code, 0);
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
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  RegistrationRequest registration;
  registration.app_id = app_id;
  registration.version = version;
  base::RunLoop loop;
  update_service->RegisterApp(registration,
                              base::BindLambdaForTesting([&loop](int result) {
                                EXPECT_EQ(result, 0);
                                loop.Quit();
                              }));
  loop.Run();
}

void UninstallApp(UpdaterScope scope, const std::string& app_id) {
  // This can probably be combined with mac into integration_tests_posix.cc.
  SetExistenceCheckerPath(scope, app_id,
                          base::FilePath(FILE_PATH_LITERAL("NONE")));
}

base::CommandLine MakeElevated(base::CommandLine command_line) {
  command_line.PrependWrapper("/usr/bin/sudo");
  return command_line;
}

}  // namespace updater::test
