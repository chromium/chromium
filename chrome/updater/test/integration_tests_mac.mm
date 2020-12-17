// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/version.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/launchd_util.h"
#import "chrome/updater/mac/util.h"
#include "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_tests.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/test/test_app/test_app_version.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace test {

// crbug.com/1112527: These tests are not compatible with component build.
#if !defined(COMPONENT_BUILD)

namespace {

void RemoveJobFromLaunchd(Launchd::Domain domain,
                          Launchd::Type type,
                          base::ScopedCFTypeRef<CFStringRef> name) {
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(domain, type, name))
      << "Failed to delete plist for " << name;

  // Return value is ignored, since RemoveJob returns false if the job already
  // doesn't exist.
  Launchd::GetInstance()->RemoveJob(base::SysCFStringRefToUTF8(name));
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

base::FilePath GetProductPath() {
  return base::mac::GetUserLibraryPath()
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

void ExpectServiceAbsent(const std::string& service) {
  bool success = false;
  base::RunLoop loop;
  PollLaunchctlList(service, LaunchctlPresence::kAbsent,
                    base::TimeDelta::FromSeconds(7),
                    base::BindLambdaForTesting([&](bool result) {
                      success = result;
                      loop.QuitClosure().Run();
                    }));
  loop.Run();
  EXPECT_TRUE(success) << service << " is unexpectedly present.";
}

}  // namespace

base::FilePath GetDataDirPath() {
  return base::mac::GetUserLibraryPath()
      .AppendASCII("Application Support")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

void Clean() {
  EXPECT_TRUE(base::DeletePathRecursively(GetProductPath()));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      Launchd::User, Launchd::Agent, updater::CopyWakeLaunchdName()));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      Launchd::User, Launchd::Agent,
      updater::CopyUpdateServiceInternalLaunchdName()));
  EXPECT_TRUE(Launchd::GetInstance()->DeletePlist(
      Launchd::User, Launchd::Agent, updater::CopyUpdateServiceLaunchdName()));
  EXPECT_TRUE(base::DeletePathRecursively(GetDataDirPath()));

  @autoreleasepool {
    NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
    [userDefaults
        removeObjectForKey:[NSString stringWithUTF8String:kDevOverrideKeyUrl]];
    [userDefaults
        removeObjectForKey:[NSString
                               stringWithUTF8String:kDevOverrideKeyUseCUP]];

    // TODO(crbug.com/1096654): support machine case (Launchd::Domain::Local and
    // Launchd::Type::Daemon).
    RemoveJobFromLaunchd(Launchd::Domain::User, Launchd::Type::Agent,
                         CopyUpdateServiceLaunchdName());
    RemoveJobFromLaunchd(Launchd::Domain::User, Launchd::Type::Agent,
                         CopyUpdateServiceInternalLaunchdName());
  }
}

void ExpectClean() {
  // Files must not exist on the file system.
  EXPECT_FALSE(base::PathExists(GetProductPath()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, updater::CopyWakeLaunchdName()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent,
      updater::CopyUpdateServiceInternalLaunchdName()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, updater::CopyUpdateServiceLaunchdName()));
  EXPECT_FALSE(base::PathExists(GetDataDirPath()));
  ExpectServiceAbsent(kUpdateServiceLaunchdName);
  ExpectServiceAbsent(kUpdateServiceInternalLaunchdName);
}

void EnterTestMode() {
  // TODO(crbug.com/1119857): Point this to an actual fake server.
  @autoreleasepool {
    NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
    [userDefaults setURL:[NSURL URLWithString:@"http://localhost:8367"]
                  forKey:[NSString stringWithUTF8String:kDevOverrideKeyUrl]];
    [userDefaults
        setBool:NO
         forKey:[NSString stringWithUTF8String:kDevOverrideKeyUseCUP]];
  }
}

void ExpectInstalled() {
  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(GetProductPath()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(Launchd::User, Launchd::Agent,
                                                  CopyWakeLaunchdName()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, CopyUpdateServiceInternalLaunchdName()));
}

void Install() {
  const base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

void ExpectActive() {
  // Files must exist on the file system.
  EXPECT_TRUE(base::PathExists(GetProductPath()));
  EXPECT_TRUE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, CopyUpdateServiceLaunchdName()));
}

void RegisterTestApp() {
  const base::FilePath path = GetTestAppExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kRegisterUpdaterSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

base::FilePath GetInstalledExecutablePath() {
  return GetUpdaterExecutablePath();
}

void ExpectCandidateUninstalled() {
  base::FilePath versioned_folder_path = GetVersionedUpdaterFolderPath();
  EXPECT_FALSE(base::PathExists(versioned_folder_path));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, CopyWakeLaunchdName()));
  EXPECT_FALSE(Launchd::GetInstance()->PlistExists(
      Launchd::User, Launchd::Agent, CopyUpdateServiceInternalLaunchdName()));
}

void Uninstall() {
  // Copy logs from GetDataDirPath() before updater uninstalls itself
  // and deletes the path.
  CopyLog(GetDataDirPath());

  // Uninstall the updater.
  const base::FilePath path = GetExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kUninstallSwitch);
  int exit_code = -1;
  ASSERT_TRUE(Run(command_line, &exit_code));
  EXPECT_EQ(0, exit_code);
}

base::FilePath GetFakeUpdaterInstallFolderPath(const base::Version& version) {
  return GetExecutableFolderPathForVersion(version);
}

#endif  // !defined(COMPONENT_BUILD)

}  // namespace test
}  // namespace updater
