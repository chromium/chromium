// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/strcat_win.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/shortcut.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/taskbar_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class InstallShortcutTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(com_initializer_.Succeeded());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.GetPath().Append(installer::kChromeExe);
    EXPECT_TRUE(base::WriteFile(chrome_exe_, ""));

    ShellUtil::ShortcutProperties chrome_properties(ShellUtil::CURRENT_USER);
    ShellUtil::AddDefaultShortcutProperties(chrome_exe_, &chrome_properties);

    expected_properties_.set_target(chrome_exe_);
    expected_properties_.set_icon(chrome_properties.icon,
                                  chrome_properties.icon_index);
    expected_properties_.set_app_id(chrome_properties.app_id);
    expected_properties_.set_description(chrome_properties.description);
    expected_start_menu_properties_ = expected_properties_;

    prefs_.reset(GetFakeInitialPrefs(false, false));

    ASSERT_TRUE(fake_user_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_start_menu_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_start_menu_.CreateUniqueTempDir());
    user_desktop_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_USER_DESKTOP, fake_user_desktop_.GetPath());
    common_desktop_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_COMMON_DESKTOP, fake_common_desktop_.GetPath());
    user_quick_launch_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_USER_QUICK_LAUNCH, fake_user_quick_launch_.GetPath());
    start_menu_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_START_MENU, fake_start_menu_.GetPath());
    common_start_menu_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_COMMON_START_MENU, fake_common_start_menu_.GetPath());

    std::wstring shortcut_name(InstallUtil::GetShortcutName() +
                               installer::kLnkExt);

    user_desktop_shortcut_ = fake_user_desktop_.GetPath().Append(shortcut_name);
    user_quick_launch_shortcut_ =
        fake_user_quick_launch_.GetPath().Append(shortcut_name);
    user_start_menu_shortcut_ =
        fake_start_menu_.GetPath().Append(shortcut_name);
    system_desktop_shortcut_ =
        fake_common_desktop_.GetPath().Append(shortcut_name);
    system_start_menu_shortcut_ =
        fake_common_start_menu_.GetPath().Append(shortcut_name);
  }

  void TearDown() override {
    // Try to unpin potentially pinned shortcuts (although pinning isn't tested,
    // the call itself might still have pinned the Start Menu shortcuts).
    UnpinShortcutFromTaskbar(user_start_menu_shortcut_);
    UnpinShortcutFromTaskbar(system_start_menu_shortcut_);
  }

  installer::InitialPreferences* GetFakeInitialPrefs(
      bool do_not_create_desktop_shortcut,
      bool do_not_create_quick_launch_shortcut) {
    const struct {
      const char* pref_name;
      bool is_desired;
    } desired_prefs[] = {
        {installer::initial_preferences::kDoNotCreateDesktopShortcut,
         do_not_create_desktop_shortcut},
        {installer::initial_preferences::kDoNotCreateQuickLaunchShortcut,
         do_not_create_quick_launch_shortcut},
    };

    std::string initial_prefs("{\"distribution\":{");
    for (size_t i = 0; i < std::size(desired_prefs); ++i) {
      initial_prefs += (i == 0 ? "\"" : ",\"");
      initial_prefs += UNSAFE_TODO(desired_prefs[i]).pref_name;
      initial_prefs += "\":";
      initial_prefs += base::ToString(UNSAFE_TODO(desired_prefs[i]).is_desired);
    }
    initial_prefs += "}}";

    return new installer::InitialPreferences(initial_prefs);
  }

  base::win::ScopedCOMInitializer com_initializer_;

  base::win::ShortcutProperties expected_properties_;
  base::win::ShortcutProperties expected_start_menu_properties_;

  base::FilePath chrome_exe_;
  std::unique_ptr<installer::InitialPreferences> prefs_;

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
  base::FilePath system_desktop_shortcut_;
  base::FilePath system_start_menu_shortcut_;
  base::FilePath system_start_menu_subdir_shortcut_;
};

}  // namespace

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST(OsUpdateHandlerCmdTest, OsUpdated) {
  constexpr wchar_t kInstalledVersion[] = L"128.0.0.0";
  constexpr wchar_t kLastWindowsVersion[] = L"1.1.1.1";
  constexpr wchar_t kCurWindowsVersion[] = L"1.1.1.2";
  base::CommandLine setup_command_line(base::CommandLine::NO_PROGRAM);
  base::FilePath path(L"c:\\tmp");
  installer::InstallerState system_level_installer_state(
      installer::InstallerState::SYSTEM_LEVEL);
  system_level_installer_state.set_target_path_for_testing(path);
  setup_command_line.ParseFromString(base::StrCat(
      {L"c:\\tmp\\setup.exe ", kLastWindowsVersion, L"-", kCurWindowsVersion}));

  // Test system-level install command line.
  auto cmd_line = installer::GetOsUpdateHandlerCommand(
      system_level_installer_state, kInstalledVersion, setup_command_line);
  EXPECT_TRUE(cmd_line.has_value());
  std::wstring expected_cmd_line =
      base::StrCat({L"\"", path.value(), L"\\", kInstalledVersion, L"\\",
                    installer::kOsUpdateHandlerExe, L"\" --",
                    base::ASCIIToWide(installer::switches::kSystemLevel), L" ",
                    kLastWindowsVersion, L"-", kCurWindowsVersion});
  EXPECT_EQ(expected_cmd_line, cmd_line->GetCommandLineString());

  // Test user-level install command line.
  installer::InstallerState user_level_installer_state(
      installer::InstallerState::USER_LEVEL);
  user_level_installer_state.set_target_path_for_testing(path);
  cmd_line = installer::GetOsUpdateHandlerCommand(
      user_level_installer_state, kInstalledVersion, setup_command_line);
  EXPECT_TRUE(cmd_line.has_value());
  expected_cmd_line =
      base::StrCat({L"\"", path.value(), L"\\", kInstalledVersion, L"\\",
                    installer::kOsUpdateHandlerExe, L"\" ", kLastWindowsVersion,
                    L"-", kCurWindowsVersion});
  EXPECT_EQ(expected_cmd_line, cmd_line->GetCommandLineString());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

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
  std::unique_ptr<installer::InitialPreferences> prefs_no_desktop(
      GetFakeInitialPrefs(true, false));
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
  std::unique_ptr<installer::InitialPreferences> prefs_no_ql(
      GetFakeInitialPrefs(false, true));
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
      base::win::ShortcutOperation::kCreateAlways));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      user_quick_launch_shortcut_, dummy_properties,
      base::win::ShortcutOperation::kCreateAlways));
  ASSERT_TRUE(base::CreateDirectory(user_start_menu_shortcut_.DirName()));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      user_start_menu_shortcut_, dummy_properties,
      base::win::ShortcutOperation::kCreateAlways));

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
      base::win::ShortcutOperation::kCreateAlways));
  ASSERT_TRUE(base::CreateDirectory(user_start_menu_shortcut_.DirName()));

  installer::CreateOrUpdateShortcuts(
      chrome_exe_, *prefs_, installer::CURRENT_USER,
      installer::INSTALL_SHORTCUT_REPLACE_EXISTING);
  base::win::ValidateShortcut(user_desktop_shortcut_, expected_properties_);
  ASSERT_FALSE(base::PathExists(user_quick_launch_shortcut_));
  ASSERT_FALSE(base::PathExists(user_start_menu_shortcut_));
}

TEST_F(InstallShortcutTest, CreateIfNoSystemLevelAllSystemShortcutsExist) {
  base::win::ShortcutProperties dummy_properties;
  base::FilePath dummy_target;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &dummy_target));
  dummy_properties.set_target(dummy_target);

  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      system_desktop_shortcut_, dummy_properties,
      base::win::ShortcutOperation::kCreateAlways));
  ASSERT_TRUE(base::CreateDirectory(system_start_menu_shortcut_.DirName()));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      system_start_menu_shortcut_, dummy_properties,
      base::win::ShortcutOperation::kCreateAlways));

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
      base::win::ShortcutOperation::kCreateAlways));

  installer::CreateOrUpdateShortcuts(
      chrome_exe_, *prefs_, installer::CURRENT_USER,
      installer::INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL);
  ASSERT_FALSE(base::PathExists(user_desktop_shortcut_));
  base::win::ValidateShortcut(user_quick_launch_shortcut_,
                              expected_properties_);
  base::win::ValidateShortcut(user_start_menu_shortcut_,
                              expected_start_menu_properties_);
}
