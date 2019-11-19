// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install.h"

#include <objbase.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/version.h"
#include "base/win/shortcut.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A parameterized test harness for testing
// installer::CreateVisualElementsManifest. The parameters are:
// 0: an index into a brand's install_static::kInstallModes array.
// 1: the expected manifest.
class CreateVisualElementsManifestTest
    : public ::testing::TestWithParam<
          std::tuple<install_static::InstallConstantIndex, const char*>> {
 protected:
  CreateVisualElementsManifestTest()
      : scoped_install_details_(false /* !system_level */,
                                std::get<0>(GetParam())),
        start_menu_override_(base::DIR_START_MENU),
        expected_manifest_(std::get<1>(GetParam())),
        version_("0.0.0.0") {}

  void SetUp() override {
    // Create a temp directory for testing.
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

    version_dir_ = test_dir_.GetPath().AppendASCII(version_.GetString());
    ASSERT_TRUE(base::CreateDirectory(version_dir_));

    manifest_path_ =
        test_dir_.GetPath().Append(installer::kVisualElementsManifest);

    ASSERT_TRUE(base::PathService::Get(base::DIR_START_MENU,
                                       &start_menu_shortcut_path_));
    start_menu_shortcut_path_ = start_menu_shortcut_path_.Append(
        installer::GetLocalizedString(IDS_PRODUCT_NAME_BASE) +
        installer::kLnkExt);
  }

  void TearDown() override {
    // Clean up test directory manually so we can fail if it leaks.
    ASSERT_TRUE(test_dir_.Delete());
  }

  // Creates a dummy test file at |path|.
  void CreateTestFile(const base::FilePath& path) {
    static constexpr char kBlah[] = "blah";
    ASSERT_EQ(static_cast<int>(base::size(kBlah) - 1),
              base::WriteFile(path, &kBlah[0], base::size(kBlah) - 1));
  }

  // Creates the VisualElements directory and a light asset, if testing such.
  void PrepareTestVisualElementsDirectory() {
    base::FilePath visual_elements_dir =
        version_dir_.Append(installer::kVisualElements);
    ASSERT_TRUE(base::CreateDirectory(visual_elements_dir));
    base::string16 light_logo_file_name = base::StringPrintf(
        L"Logo%ls.png", install_static::InstallDetails::Get().logo_suffix());
    ASSERT_NO_FATAL_FAILURE(
        CreateTestFile(visual_elements_dir.Append(light_logo_file_name)));
  }

  // Creates a bogus file at the location of the start menu shortcut.
  void CreateStartMenuShortcut() {
    ASSERT_NO_FATAL_FAILURE(CreateTestFile(start_menu_shortcut_path_));
  }

  // InstallDetails for this test run.
  install_static::ScopedInstallDetails scoped_install_details_;

  // Override the location of the Start Menu shortcuts.
  base::ScopedPathOverride start_menu_override_;

  // The expected contents of the manifest.
  const char* const expected_manifest_;

  // A dummy version number used to create the version directory.
  const base::Version version_;

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir test_dir_;

  // The path to |test_dir_|\|version_|.
  base::FilePath version_dir_;

  // The path to VisualElementsManifest.xml.
  base::FilePath manifest_path_;

  // The path to the Start Menu shortcut.
  base::FilePath start_menu_shortcut_path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateVisualElementsManifestTest);
};

constexpr char kExpectedPrimaryManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\Logo.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogo.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogo.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kExpectedBetaManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\LogoBeta.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogoBeta.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogoBeta.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

constexpr char kExpectedDevManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\LogoDev.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogoDev.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogoDev.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

constexpr char kExpectedCanaryManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\LogoCanary.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogoCanary.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogoCanary.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

INSTANTIATE_TEST_SUITE_P(
    GoogleChrome,
    CreateVisualElementsManifestTest,
    testing::Combine(testing::Values(install_static::STABLE_INDEX),
                     testing::Values(kExpectedPrimaryManifest)));
INSTANTIATE_TEST_SUITE_P(
    BetaChrome,
    CreateVisualElementsManifestTest,
    testing::Combine(testing::Values(install_static::BETA_INDEX),
                     testing::Values(kExpectedBetaManifest)));
INSTANTIATE_TEST_SUITE_P(
    DevChrome,
    CreateVisualElementsManifestTest,
    testing::Combine(testing::Values(install_static::DEV_INDEX),
                     testing::Values(kExpectedDevManifest)));
INSTANTIATE_TEST_SUITE_P(
    CanaryChrome,
    CreateVisualElementsManifestTest,
    testing::Combine(testing::Values(install_static::CANARY_INDEX),
                     testing::Values(kExpectedCanaryManifest)));
#else
INSTANTIATE_TEST_SUITE_P(
    Chromium,
    CreateVisualElementsManifestTest,
    testing::Combine(testing::Values(install_static::CHROMIUM_INDEX),
                     testing::Values(kExpectedPrimaryManifest)));
#endif

class InstallShortcutTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_EQ(S_OK, CoInitialize(NULL));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.GetPath().Append(installer::kChromeExe);
    EXPECT_EQ(0, base::WriteFile(chrome_exe_, "", 0));

    ShellUtil::ShortcutProperties chrome_properties(ShellUtil::CURRENT_USER);
    ShellUtil::AddDefaultShortcutProperties(chrome_exe_, &chrome_properties);

    expected_properties_.set_target(chrome_exe_);
    expected_properties_.set_icon(chrome_properties.icon,
                                  chrome_properties.icon_index);
    expected_properties_.set_app_id(chrome_properties.app_id);
    expected_properties_.set_description(chrome_properties.description);
    expected_properties_.set_dual_mode(false);
    expected_start_menu_properties_ = expected_properties_;
    expected_start_menu_properties_.set_dual_mode(false);

    prefs_.reset(GetFakeMasterPrefs(false, false));

    ASSERT_TRUE(fake_user_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_start_menu_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_start_menu_.CreateUniqueTempDir());
    user_desktop_override_.reset(new base::ScopedPathOverride(
        base::DIR_USER_DESKTOP, fake_user_desktop_.GetPath()));
    common_desktop_override_.reset(new base::ScopedPathOverride(
        base::DIR_COMMON_DESKTOP, fake_common_desktop_.GetPath()));
    user_quick_launch_override_.reset(new base::ScopedPathOverride(
        base::DIR_USER_QUICK_LAUNCH, fake_user_quick_launch_.GetPath()));
    start_menu_override_.reset(new base::ScopedPathOverride(
        base::DIR_START_MENU, fake_start_menu_.GetPath()));
    common_start_menu_override_.reset(new base::ScopedPathOverride(
        base::DIR_COMMON_START_MENU, fake_common_start_menu_.GetPath()));

    base::string16 shortcut_name(InstallUtil::GetShortcutName() +
                                 installer::kLnkExt);

    user_desktop_shortcut_ = fake_user_desktop_.GetPath().Append(shortcut_name);
    user_quick_launch_shortcut_ =
        fake_user_quick_launch_.GetPath().Append(shortcut_name);
    user_start_menu_shortcut_ =
        fake_start_menu_.GetPath().Append(shortcut_name);
    user_start_menu_subdir_shortcut_ =
        fake_start_menu_.GetPath()
            .Append(InstallUtil::GetChromeShortcutDirNameDeprecated())
            .Append(shortcut_name);
    system_desktop_shortcut_ =
        fake_common_desktop_.GetPath().Append(shortcut_name);
    system_start_menu_shortcut_ =
        fake_common_start_menu_.GetPath().Append(shortcut_name);
    system_start_menu_subdir_shortcut_ =
        fake_common_start_menu_.GetPath()
            .Append(InstallUtil::GetChromeShortcutDirNameDeprecated())
            .Append(shortcut_name);
  }

  void TearDown() override {
    // Try to unpin potentially pinned shortcuts (although pinning isn't tested,
    // the call itself might still have pinned the Start Menu shortcuts).
    base::win::UnpinShortcutFromTaskbar(user_start_menu_shortcut_);
    base::win::UnpinShortcutFromTaskbar(user_start_menu_subdir_shortcut_);
    base::win::UnpinShortcutFromTaskbar(system_start_menu_shortcut_);
    base::win::UnpinShortcutFromTaskbar(system_start_menu_subdir_shortcut_);
    CoUninitialize();
  }

  installer::MasterPreferences* GetFakeMasterPrefs(
      bool do_not_create_desktop_shortcut,
      bool do_not_create_quick_launch_shortcut) {
    const struct {
      const char* pref_name;
      bool is_desired;
    } desired_prefs[] = {
      { installer::master_preferences::kDoNotCreateDesktopShortcut,
        do_not_create_desktop_shortcut },
      { installer::master_preferences::kDoNotCreateQuickLaunchShortcut,
        do_not_create_quick_launch_shortcut },
    };

    std::string master_prefs("{\"distribution\":{");
    for (size_t i = 0; i < base::size(desired_prefs); ++i) {
      master_prefs += (i == 0 ? "\"" : ",\"");
      master_prefs += desired_prefs[i].pref_name;
      master_prefs += "\":";
      master_prefs += desired_prefs[i].is_desired ? "true" : "false";
    }
    master_prefs += "}}";

    return new installer::MasterPreferences(master_prefs);
  }

  base::win::ShortcutProperties expected_properties_;
  base::win::ShortcutProperties expected_start_menu_properties_;

  base::FilePath chrome_exe_;
  std::unique_ptr<installer::MasterPreferences> prefs_;

  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir fake_user_desktop_;
  base::ScopedTempDir fake_common_desktop_;
  base::ScopedTempDir fake_user_quick_launch_;
  base::ScopedTempDir fake_start_menu_;
  base::ScopedTempDir fake_common_start_menu_;
  std::unique_ptr<base::ScopedPathOverride> user_desktop_override_;
  std::unique_ptr<base::ScopedPathOverride> common_desktop_override_;
  std::unique_ptr<base::ScopedPathOverride> user_quick_launch_override_;
  std::unique_ptr<base::ScopedPathOverride> start_menu_override_;
  std::unique_ptr<base::ScopedPathOverride> common_start_menu_override_;

  base::FilePath user_desktop_shortcut_;
  base::FilePath user_quick_launch_shortcut_;
  base::FilePath user_start_menu_shortcut_;
  base::FilePath user_start_menu_subdir_shortcut_;
  base::FilePath system_desktop_shortcut_;
  base::FilePath system_start_menu_shortcut_;
  base::FilePath system_start_menu_subdir_shortcut_;
};

}  // namespace

// Test that VisualElementsManifest.xml is not created when VisualElements are
// not present.
TEST_P(CreateVisualElementsManifestTest, VisualElementsManifestNotCreated) {
  ASSERT_TRUE(
      installer::CreateVisualElementsManifest(test_dir_.GetPath(), version_));
  ASSERT_FALSE(base::PathExists(manifest_path_));
}

// Test that VisualElementsManifest.xml is created with the correct content when
// VisualElements are present.
TEST_P(CreateVisualElementsManifestTest, VisualElementsManifestCreated) {
  ASSERT_NO_FATAL_FAILURE(PrepareTestVisualElementsDirectory());
  ASSERT_TRUE(
      installer::CreateVisualElementsManifest(test_dir_.GetPath(), version_));
  ASSERT_TRUE(base::PathExists(manifest_path_));

  std::string read_manifest;
  ASSERT_TRUE(base::ReadFileToString(manifest_path_, &read_manifest));

  ASSERT_STREQ(expected_manifest_, read_manifest.c_str());
}

TEST_F(InstallShortcutTest, CreateAllShortcuts) {
  installer::CreateOrUpdateShortcuts(chrome_exe_, *prefs_,
                                     installer::CURRENT_USER,
                                     installer::INSTALL_SHORTCUT_CREATE_ALL);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_start_menu_properties_);
}

TEST_F(InstallShortcutTest, CreateAllShortcutsSystemLevel) {
  installer::CreateOrUpdateShortcuts(chrome_exe_, *prefs_, installer::ALL_USERS,
                                     installer::INSTALL_SHORTCUT_CREATE_ALL);
  base::win::ValidateShortcut(system_desktop_shortcut_, expected_properties_);
  base::win::ValidateShortcut(system_start_menu_shortcut_,
                              expected_start_menu_properties_);
  // The quick launch shortcut is always created per-user for the admin running
  // the install (other users will get it via Active Setup).
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
}

TEST_F(InstallShortcutTest, CreateAllShortcutsButDesktopShortcut) {
  std::unique_ptr<installer::MasterPreferences> prefs_no_desktop(
      GetFakeMasterPrefs(true, false));
  installer::CreateOrUpdateShortcuts(chrome_exe_, *prefs_no_desktop,
                                     installer::CURRENT_USER,
                                     installer::INSTALL_SHORTCUT_CREATE_ALL);
  ASSERT_FALSE(base::PathExists(user_desktop_shortcut_));
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_start_menu_properties_);
}

TEST_F(InstallShortcutTest, CreateAllShortcutsButQuickLaunchShortcut) {
  std::unique_ptr<installer::MasterPreferences> prefs_no_ql(
      GetFakeMasterPrefs(false, true));
  installer::CreateOrUpdateShortcuts(chrome_exe_, *prefs_no_ql,
                                     installer::CURRENT_USER,
                                     installer::INSTALL_SHORTCUT_CREATE_ALL);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  ASSERT_FALSE(base::PathExists(user_quick_launch_shortcut_));
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_start_menu_properties_);
}

TEST_F(InstallShortcutTest, ReplaceAll) {
  base::win::ShortcutProperties dummy_properties;
  base::FilePath dummy_target;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &dummy_target));
  dummy_properties.set_target(dummy_target);
  dummy_properties.set_working_dir(fake_user_desktop_.GetPath());
  dummy_properties.set_arguments(L"--dummy --args");
  dummy_properties.set_app_id(L"El.Dummiest");

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_desktop_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_quick_launch_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::CreateDirectory(user_start_menu_shortcut_.DirName()));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_start_menu_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));

  installer::CreateOrUpdateShortcuts(
      chrome_exe_, *prefs_, installer::CURRENT_USER,
      installer::INSTALL_SHORTCUT_REPLACE_EXISTING);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_start_menu_properties_);
}

TEST_F(InstallShortcutTest, ReplaceExisting) {
  base::win::ShortcutProperties dummy_properties;
  base::FilePath dummy_target;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &dummy_target));
  dummy_properties.set_target(dummy_target);
  dummy_properties.set_working_dir(fake_user_desktop_.GetPath());
  dummy_properties.set_arguments(L"--dummy --args");
  dummy_properties.set_app_id(L"El.Dummiest");

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  user_desktop_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::CreateDirectory(user_start_menu_shortcut_.DirName()));

  installer::CreateOrUpdateShortcuts(
      chrome_exe_, *prefs_, installer::CURRENT_USER,
      installer::INSTALL_SHORTCUT_REPLACE_EXISTING);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  ASSERT_FALSE(base::PathExists(user_quick_launch_shortcut_));
  ASSERT_FALSE(base::PathExists(user_start_menu_shortcut_));
}

class MigrateShortcutTest : public InstallShortcutTest,
                            public testing::WithParamInterface<
                                testing::tuple<
                                    installer::InstallShortcutOperation,
                                    installer::InstallShortcutLevel>> {
 public:
  MigrateShortcutTest() : shortcut_operation_(testing::get<0>(GetParam())),
                          shortcut_level_(testing::get<1>(GetParam())) {}

 protected:
  const installer::InstallShortcutOperation shortcut_operation_;
  const installer::InstallShortcutLevel shortcut_level_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrateShortcutTest);
};

TEST_P(MigrateShortcutTest, MigrateAwayFromDeprecatedStartMenuTest) {
  base::win::ShortcutProperties dummy_properties;
  base::FilePath dummy_target;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &dummy_target));
  dummy_properties.set_target(expected_properties_.target);
  dummy_properties.set_working_dir(fake_user_desktop_.GetPath());
  dummy_properties.set_arguments(L"--dummy --args");
  dummy_properties.set_app_id(L"El.Dummiest");

  base::FilePath start_menu_shortcut;
  base::FilePath start_menu_subdir_shortcut;
  if (shortcut_level_ == installer::CURRENT_USER) {
    start_menu_shortcut = user_start_menu_shortcut_;
    start_menu_subdir_shortcut = user_start_menu_subdir_shortcut_;
  } else {
    start_menu_shortcut = system_start_menu_shortcut_;
    start_menu_subdir_shortcut = system_start_menu_subdir_shortcut_;
  }

  ASSERT_TRUE(base::CreateDirectory(start_menu_subdir_shortcut.DirName()));
  ASSERT_FALSE(base::PathExists(start_menu_subdir_shortcut));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  start_menu_subdir_shortcut, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(start_menu_subdir_shortcut));
  ASSERT_FALSE(base::PathExists(start_menu_shortcut));

  installer::CreateOrUpdateShortcuts(chrome_exe_, *prefs_, shortcut_level_,
                                     shortcut_operation_);
  ASSERT_FALSE(base::PathExists(start_menu_subdir_shortcut));
  ASSERT_TRUE(base::PathExists(start_menu_shortcut));
}

// Verify that any installer operation for any installation level triggers
// the migration from sub-folder to root of start-menu.
INSTANTIATE_TEST_SUITE_P(
    MigrateShortcutTests,
    MigrateShortcutTest,
    testing::Combine(
        testing::Values(
            installer::INSTALL_SHORTCUT_REPLACE_EXISTING,
            installer::INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL,
            installer::INSTALL_SHORTCUT_CREATE_ALL),
        testing::Values(installer::CURRENT_USER, installer::ALL_USERS)));

TEST_F(InstallShortcutTest, CreateIfNoSystemLevelAllSystemShortcutsExist) {
  base::win::ShortcutProperties dummy_properties;
  base::FilePath dummy_target;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &dummy_target));
  dummy_properties.set_target(dummy_target);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  system_desktop_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::CreateDirectory(
        system_start_menu_shortcut_.DirName()));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  system_start_menu_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));

  installer::CreateOrUpdateShortcuts(
      chrome_exe_, *prefs_, installer::CURRENT_USER,
      installer::INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL);
  ASSERT_FALSE(base::PathExists(user_desktop_shortcut_));
  ASSERT_FALSE(base::PathExists(user_start_menu_shortcut_));
  // There is no system-level quick launch shortcut, so creating the user-level
  // one should always succeed.
  ASSERT_TRUE(base::PathExists(user_quick_launch_shortcut_));
}

TEST_F(InstallShortcutTest, CreateIfNoSystemLevelNoSystemShortcutsExist) {
  installer::CreateOrUpdateShortcuts(
      chrome_exe_, *prefs_, installer::CURRENT_USER,
      installer::INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_start_menu_properties_);
}

TEST_F(InstallShortcutTest, CreateIfNoSystemLevelSomeSystemShortcutsExist) {
  base::win::ShortcutProperties dummy_properties;
  base::FilePath dummy_target;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &dummy_target));
  dummy_properties.set_target(dummy_target);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
                  system_desktop_shortcut_, dummy_properties,
                  base::win::SHORTCUT_CREATE_ALWAYS));

  installer::CreateOrUpdateShortcuts(
      chrome_exe_, *prefs_, installer::CURRENT_USER,
      installer::INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL);
  ASSERT_FALSE(base::PathExists(user_desktop_shortcut_));
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_start_menu_properties_);
}
