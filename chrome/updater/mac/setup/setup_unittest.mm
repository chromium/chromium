// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/mac/install_from_archive.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater_setup {

namespace {

constexpr char kTestAppNameWithExtension[] = "InstallerTest.app";
constexpr char kTestAppFrameworkName[] = "InstallerTest Framework.framework";
constexpr char kTestAppVersion[] = "0";
constexpr char kTestBundleId[] = "com.install.test";
constexpr char kTestDirName[] = "InstallerTestDir";
constexpr char kUpdaterTestDMGName[] = "updater_setup_test_dmg.dmg";

void CreateTestApp(const base::FilePath& test_dir) {
  // Create file paths for each part of the app we want to create.
  base::FilePath test_app_path =
      test_dir.Append(FILE_PATH_LITERAL(kTestAppNameWithExtension));
  base::FilePath test_app_info_plist_path =
      test_app_path.Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Info.plist"));
  base::FilePath test_app_frameworks_path =
      test_app_path.Append(FILE_PATH_LITERAL("Contents"))
          .Append(FILE_PATH_LITERAL("Frameworks"))
          .Append(FILE_PATH_LITERAL(kTestAppFrameworkName));
  base::FilePath test_app_versions_path =
      test_app_frameworks_path.Append(FILE_PATH_LITERAL("Versions"));
  base::FilePath test_app_current_path =
      test_app_versions_path.Append(FILE_PATH_LITERAL("Current"));
  base::FilePath test_app_versioned_path =
      test_app_versions_path.Append(FILE_PATH_LITERAL(kTestAppVersion));
  base::FilePath test_app_versioned_resources_path =
      test_app_versioned_path.Append(FILE_PATH_LITERAL("Resources"));

  // First create the directory all the way up to Resources. We only need to do
  // this once because it'll create everything recursively.
  ASSERT_FALSE(base::PathExists(test_app_versioned_resources_path));
  ASSERT_TRUE(base::CreateDirectory(test_app_versioned_resources_path));

  // Now we should also create the Current dir to prepare for symlinks.
  ASSERT_FALSE(base::PathExists(test_app_current_path));
  ASSERT_TRUE(base::CreateDirectory(test_app_current_path));

  // Now to create some symlinks. We need to create one from versioned directory
  // to Frameworks, and one from versioned directory to the Current directory.
  ASSERT_TRUE(base::CreateSymbolicLink(
      test_app_versioned_resources_path,
      test_app_current_path.Append(FILE_PATH_LITERAL("Resources"))));

  ASSERT_TRUE(base::CreateSymbolicLink(
      test_app_versioned_resources_path,
      test_app_frameworks_path.Append(FILE_PATH_LITERAL("Resources"))));

  // Now to create the info plist.
  NSDictionary* launchd_plist = @{
    @"CFBundleShortVersionString" : base::SysUTF8ToNSString(kTestAppVersion),
    @"CFBundleIdentifier" : base::SysUTF8ToNSString(kTestBundleId)
  };

  ASSERT_TRUE([launchd_plist
      writeToURL:base::apple::FilePathToNSURL(test_app_info_plist_path)
           error:nil]);
}

void CreateTestSuiteTestDir(const base::FilePath& test_dir) {
  // Now lets copy the dmg into the test directory.
  // Get test data path.
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));

  // Get path to the dmg.
  base::FilePath dmg_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .Append(FILE_PATH_LITERAL(kUpdaterTestDMGName));
  ASSERT_TRUE(base::PathExists(dmg_path));

  // Copy the dmg over to the test directory.
  base::FilePath dest_path =
      test_dir.Append(FILE_PATH_LITERAL(kUpdaterTestDMGName));
  ASSERT_TRUE(base::CopyFile(dmg_path, dest_path));

  // Now to create a fake test app.
  CreateTestApp(test_dir);
}

base::ScopedTempDir& GetTestSuiteScopedTempDir() {
  static base::NoDestructor<base::ScopedTempDir> test_suite_dir_;
  return *test_suite_dir_;
}

const base::FilePath& GetTestSuiteDirPath() {
  return GetTestSuiteScopedTempDir().GetPath();
}

}  // namespace

class ChromeUpdaterMacSetupTest : public testing::Test {
 public:
  ~ChromeUpdaterMacSetupTest() override = default;

  static void SetUpTestSuite() {
    // SetUpTestSuite will run a script (install_test_helper.sh), which will set
    // up all the necessary things for running the mac installer test. This will
    // include creating dummy *.app to test the update and a sample dmg that
    // will be used as well.
    ASSERT_TRUE(GetTestSuiteScopedTempDir().CreateUniqueTempDir());
    CreateTestSuiteTestDir(GetTestSuiteDirPath());
  }

  static void TearDownTestSuite() {
    // TearDownTestSuite will run a script (install_test_helper.sh), to clean up
    // anything remaining from the test. This will need to be run with an arg
    // "clean" after the normal args.
    ASSERT_TRUE(GetTestSuiteScopedTempDir().Delete());
  }

  void SetUp() override {
    // Copies the directory created in set up test suite corresponding to each
    // test case name. This way, we have unique directories that we can set up
    // and clean up per test.
    base::FilePath temp_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));

    test_dir_ = temp_dir.Append(
        FILE_PATH_LITERAL(base::StrCat({kTestDirName, "-",
                                        ::testing::UnitTest::GetInstance()
                                            ->current_test_info()
                                            ->test_suite_name()})));
    ASSERT_TRUE(base::CopyDirectory(GetTestSuiteDirPath(), test_dir_, true));
  }

  void TearDown() override {
    ASSERT_TRUE(base::DeletePathRecursively(test_dir_));
  }

  base::FilePath GetTestDir() { return test_dir_; }

 private:
  base::FilePath test_dir_;
};

TEST_F(ChromeUpdaterMacSetupTest, InstallFromArchiveNoArgs) {
  // Get the path of the dmg based on the test directory path and validate it
  // exists.
  const base::FilePath dmg_file_path =
      GetTestDir().Append(FILE_PATH_LITERAL(kUpdaterTestDMGName));
  ASSERT_TRUE(base::PathExists(dmg_file_path));
  ASSERT_NE(updater::InstallFromArchive(dmg_file_path, {}, {},
                                        updater::UpdaterScope::kUser,
                                        base::Version("0"), {}, {}, false,
                                        TestTimeouts::action_timeout()),
            0);
}

TEST_F(ChromeUpdaterMacSetupTest, InstallFromArchiveWithArgsFail) {
  // Get the path of the dmg based on the test directory path and validate it
  // exists.
  const base::FilePath dmg_file_path =
      GetTestDir().Append(FILE_PATH_LITERAL(kUpdaterTestDMGName));
  ASSERT_TRUE(base::PathExists(dmg_file_path));
  ASSERT_NE(updater::InstallFromArchive(dmg_file_path, {}, {},
                                        updater::UpdaterScope::kUser,
                                        base::Version("0"), "arg2", {}, false,
                                        TestTimeouts::action_timeout()),
            0);
}

TEST_F(ChromeUpdaterMacSetupTest, InstallFromArchiveWithArgsPass) {
  // Get the path of the dmg based on the test directory path and validate it
  // exists.
  const base::FilePath dmg_file_path =
      GetTestDir().Append(FILE_PATH_LITERAL(kUpdaterTestDMGName));
  ASSERT_TRUE(base::PathExists(dmg_file_path));

  const base::FilePath installed_app_path =
      GetTestDir().Append(FILE_PATH_LITERAL(kTestAppNameWithExtension));
  ASSERT_TRUE(base::PathExists(installed_app_path));

  ASSERT_EQ(updater::InstallFromArchive(dmg_file_path, installed_app_path, {},
                                        updater::UpdaterScope::kUser,
                                        base::Version(kTestAppVersion), {}, {},
                                        false, TestTimeouts::action_timeout()),
            0);
}

TEST_F(ChromeUpdaterMacSetupTest, InstallFromArchiveWithExtraneousArgsPass) {
  // Get the path of the dmg based on the test directory path and validate it
  // exists.
  const base::FilePath dmg_file_path =
      GetTestDir().Append(FILE_PATH_LITERAL(kUpdaterTestDMGName));
  ASSERT_TRUE(base::PathExists(dmg_file_path));

  // Get the path of the installed app and then validate it exists.
  const base::FilePath installed_app_path =
      GetTestDir().Append(FILE_PATH_LITERAL(kTestAppNameWithExtension));
  ASSERT_TRUE(base::PathExists(installed_app_path));

  std::string args = base::StrCat({kTestAppVersion, " arg1 arg2"});
  ASSERT_EQ(updater::InstallFromArchive(dmg_file_path, installed_app_path, {},
                                        updater::UpdaterScope::kUser,
                                        base::Version("0"), args, {}, false,
                                        TestTimeouts::action_timeout()),
            0);
}

TEST_F(ChromeUpdaterMacSetupTest, InstallFromArchivePreinstallPostinstall) {
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  test_dir = test_dir.Append("updater");

  ASSERT_EQ(updater::InstallFromArchive(
                test_dir.Append("setup_test_envcheck").Append("marker.app"),
                base::FilePath::FromASCII("xc_path"), "ap",
                updater::UpdaterScope::kUser, base::Version("0"), "arg1 arg2",
                {}, false, TestTimeouts::action_timeout()),
            0);

  ASSERT_EQ(
      updater::InstallFromArchive(
          test_dir.Append("setup_test_preinstallfailure").Append("marker.app"),
          {}, {}, updater::UpdaterScope::kUser, base::Version("0"), {}, {},
          false, TestTimeouts::action_timeout()),
      1);

  ASSERT_EQ(
      updater::InstallFromArchive(
          test_dir.Append("setup_test_installfailure").Append("marker.app"), {},
          {}, updater::UpdaterScope::kUser, base::Version("0"), {}, {}, false,
          TestTimeouts::action_timeout()),
      2);

  ASSERT_EQ(
      updater::InstallFromArchive(
          test_dir.Append("setup_test_postinstallfailure").Append("marker.app"),
          {}, {}, updater::UpdaterScope::kUser, base::Version("0"), {}, {},
          false, TestTimeouts::action_timeout()),
      3);
}

}  // namespace updater_setup
