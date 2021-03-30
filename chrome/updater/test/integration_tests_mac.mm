// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/version.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/launchd_util.h"
#import "chrome/updater/mac/mac_util.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {
namespace test {

namespace {

Launchd::Domain LaunchdDomain(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return Launchd::Domain::Local;
    case UpdaterScope::kUser:
      return Launchd::Domain::User;
  }
}

Launchd::Type LaunchdType(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kSystem:
      return Launchd::Type::Daemon;
    case UpdaterScope::kUser:
      return Launchd::Type::Agent;
  }
}

base::FilePath GetExecutablePath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName()
      .Append(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING ".app"))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .Append(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING));
}

base::FilePath GetTestAppExecutablePath() {
  base::FilePath test_executable;
  if (!base::PathService::Get(base::FILE_EXE, &test_executable))
    return base::FilePath();
  return test_executable.DirName()
      .Append(FILE_PATH_LITERAL(TEST_APP_FULLNAME_STRING ".app"))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .Append(FILE_PATH_LITERAL(TEST_APP_FULLNAME_STRING));
}

base::Optional<base::FilePath> GetProductPath(UpdaterScope scope) {
  base::Optional<base::FilePath> path = GetLibraryFolderPath(scope);
  if (!path)
    return base::nullopt;

  return path->AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

base::Optional<base::FilePath> GetActiveFile(UpdaterScope scope,
                                             const std::string& id) {
  const base::Optional<base::FilePath> path =
      GetLibraryFolderPath(UpdaterScope::kUser);
  if (!path)
    return base::nullopt;

  return path->AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(COMPANY_SHORTNAME_STRING "SoftwareUpdate")
      .AppendASCII("Actives")
      .AppendASCII(id);
}

void ExpectServiceAbsent(UpdaterScope scope, const std::string& service) {
  VLOG(0) << __func__ << " - scope: " << scope << ". service: " << service;
  bool success = false;
  base::RunLoop loop;
  PollLaunchctlList(scope, service, LaunchctlPresence::kAbsent,
                    base::TimeDelta::FromSeconds(7),
                    base::BindLambdaForTesting([&](bool result) {
                      success = result;
                      loop.QuitClosure().Run();
                    }));
  loop.Run();
  EXPECT_TRUE(success) << service << " is unexpectedly present.";
}

}  // namespace

void EnterTestMode(const GURL& url) {
  ASSERT_TRUE(ExternalConstantsBuilder()
                  .SetUpdateURL(std::vector<std::string>{url.spec()})
                  .SetUseCUP(false)
                  .SetInitialDelay(0.1)
                  .SetServerKeepAliveSeconds(1)
                  .Overwrite());
}

base::Optional<base::FilePath> GetDataDirPath(UpdaterScope scope) {
  base::Optional<base::FilePath> app_path =
      GetApplicationSupportDirectory(scope);
  if (!app_path) {
    VLOG(1) << "Failed to get Application support path.";
    return base::nullopt;
  }

  return app_path->AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

void Clean(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  base::Optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      launchd_domain, launchd_type, updater::CopyWakeLaunchdName()));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      launchd_domain, launchd_type,
      updater::CopyUpdateServiceInternalLaunchdName()));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      launchd_domain, launchd_type, updater::CopyUpdateServiceLaunchdName()));

  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::DeletePathRecursively(*path));

  @autoreleasepool {
    RemoveJobFromLaunchd(scope, launchd_domain, launchd_type,
                         CopyWakeLaunchdName());
    RemoveJobFromLaunchd(scope, launchd_domain, launchd_type,
                         CopyUpdateServiceLaunchdName());
    RemoveJobFromLaunchd(scope, launchd_domain, launchd_type,
                         CopyUpdateServiceInternalLaunchdName());
  }
}

void ExpectClean(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  // Files must not exist on the file system.
  base::Optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type, updater::CopyWakeLaunchdName()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type,
      updater::CopyUpdateServiceInternalLaunchdName()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type, updater::CopyUpdateServiceLaunchdName()));

  path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_FALSE(base::PathExists(*path));

  ExpectServiceAbsent(scope, kUpdateServiceLaunchdName);
  ExpectServiceAbsent(scope, kUpdateServiceInternalLaunchdName);
}

void ExpectInstalled(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  // Files must exist on the file system.
  base::Optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));

  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(launchd_domain, launchd_type,
                                                  CopyWakeLaunchdName()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type, CopyUpdateServiceInternalLaunchdName()));
}

void Install(UpdaterScope scope) {
  const base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

void ExpectActiveUpdater(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  // Files must exist on the file system.
  base::Optional<base::FilePath> path = GetProductPath(scope);
  EXPECT_TRUE(path);
  if (path)
    EXPECT_TRUE(base::PathExists(*path));

  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type, CopyUpdateServiceLaunchdName()));
}

void RegisterTestApp(UpdaterScope scope) {
  const base::FilePath path = GetTestAppExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kRegisterUpdaterSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

base::Optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope) {
  return GetUpdaterExecutablePath(scope);
}

void ExpectCandidateUninstalled(UpdaterScope scope) {
  Launchd::Domain launchd_domain = LaunchdDomain(scope);
  Launchd::Type launchd_type = LaunchdType(scope);

  base::Optional<base::FilePath> versioned_folder_path =
      GetVersionedUpdaterFolderPath(scope);
  EXPECT_TRUE(versioned_folder_path);
  if (versioned_folder_path)
    EXPECT_FALSE(base::PathExists(*versioned_folder_path));

  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(launchd_domain, launchd_type,
                                                   CopyWakeLaunchdName()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      launchd_domain, launchd_type, CopyUpdateServiceInternalLaunchdName()));
}

void Uninstall(UpdaterScope scope) {
  if (::testing::Test::HasFailure())
    PrintLog(scope);
  // Copy logs from GetDataDirPath() before updater uninstalls itself
  // and deletes the path.
  base::Optional<base::FilePath> path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  if (path)
    CopyLog(*path);

  // Uninstall the updater.
  path = GetExecutablePath();
  ASSERT_TRUE(path);
  base::CommandLine command_line(*path);
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

base::Optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version) {
  return GetExecutableFolderPathForVersion(scope, version);
}

void SetActive(UpdaterScope scope, const std::string& app_id) {
  const base::Optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  VLOG(0) << "Actives file: " << *path;
  base::File::Error err = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path->DirName(), &err))
      << "Error: " << err;
  EXPECT_TRUE(base::WriteFile(*path, ""));
}

void ExpectActive(UpdaterScope scope, const std::string& app_id) {
  const base::Optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  EXPECT_TRUE(base::PathExists(*path));
  EXPECT_TRUE(base::PathIsWritable(*path));
}

void ExpectNotActive(UpdaterScope scope, const std::string& app_id) {
  const base::Optional<base::FilePath> path = GetActiveFile(scope, app_id);
  ASSERT_TRUE(path);
  EXPECT_FALSE(base::PathExists(*path));
  EXPECT_FALSE(base::PathIsWritable(*path));
}

}  // namespace test
}  // namespace updater
