// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/shell_util.h"

#include <cguid.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/synchronization/atomic_flag.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/registry.h"
#include "base/win/shortcut.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kManganeseExe[] = L"manganese.exe";
const wchar_t kIronExe[] = L"iron.exe";
const wchar_t kOtherIco[] = L"other.ico";

// For registry tests.
const wchar_t kTestProgId[] = L"TestApp";
const wchar_t kFileHandler1ProgId[] = L"FileHandler1";
const wchar_t kFileHandler2ProgId[] = L"FileHandler2";
const wchar_t kTestOpenCommand[] = L"C:\\test.exe";
const wchar_t kTestApplicationName[] = L"Test Application";
const wchar_t kTestApplicationDescription[] = L"Application Description";
const wchar_t kTestFileTypeName[] = L"Test File Type";
const wchar_t kTestIconPath[] = L"D:\\test.ico";
const wchar_t* kTestFileExtensions[] = {
    L"test1",
    L"test2",
};

// TODO(huangs): Separate this into generic shortcut tests and Chrome-specific
// tests. Specifically, we should not overly rely on getting shortcut properties
// from ShellUtil::AddDefaultShortcutProperties().
class ShellUtilShortcutTest : public testing::Test {
 protected:
  ShellUtilShortcutTest() : test_properties_(ShellUtil::CURRENT_USER) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.GetPath().Append(installer::kChromeExe);
    EXPECT_TRUE(base::WriteFile(chrome_exe_, ""));

    chrome_proxy_exe_ = temp_dir_.GetPath().Append(installer::kChromeProxyExe);
    EXPECT_TRUE(base::WriteFile(chrome_proxy_exe_, ""));

    manganese_exe_ = temp_dir_.GetPath().Append(kManganeseExe);
    EXPECT_TRUE(base::WriteFile(manganese_exe_, ""));

    iron_exe_ = temp_dir_.GetPath().Append(kIronExe);
    EXPECT_TRUE(base::WriteFile(iron_exe_, ""));

    other_ico_ = temp_dir_.GetPath().Append(kOtherIco);
    EXPECT_TRUE(base::WriteFile(other_ico_, ""));

    ASSERT_TRUE(fake_user_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_desktop_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_default_user_quick_launch_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_start_menu_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_start_menu_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_user_startup_.CreateUniqueTempDir());
    ASSERT_TRUE(fake_common_startup_.CreateUniqueTempDir());
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
    common_startup_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_COMMON_STARTUP, fake_common_startup_.GetPath());
    user_startup_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_USER_STARTUP, fake_user_startup_.GetPath());

    base::FilePath icon_path;
    base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &icon_path);
    test_properties_.set_target(chrome_exe_);
    test_properties_.set_arguments(L"--test --chrome");
    test_properties_.set_description(L"Makes polar bears dance.");
    test_properties_.set_icon(icon_path, 7);
    test_properties_.set_app_id(L"Polar.Bear");

    // The CLSID below was randomly selected.
    static constexpr CLSID toast_activator_clsid = {
        0xb2a1d927,
        0xacd1,
        0x484a,
        {0x82, 0x82, 0xd5, 0xfc, 0x66, 0x56, 0x26, 0x4b}};
    test_properties_.set_toast_activator_clsid(toast_activator_clsid);
  }

  // Returns the expected path of a test shortcut. Returns an empty FilePath on
  // failure.
  base::FilePath GetExpectedShortcutPath(
      ShellUtil::ShortcutLocation location,
      const ShellUtil::ShortcutProperties& properties) {
    base::FilePath expected_path;
    switch (location) {
      case ShellUtil::SHORTCUT_LOCATION_DESKTOP:
        expected_path = (properties.level == ShellUtil::CURRENT_USER)
                            ? fake_user_desktop_.GetPath()
                            : fake_common_desktop_.GetPath();
        break;
      case ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH:
        expected_path = fake_user_quick_launch_.GetPath();
        break;
      case ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT:
        expected_path = (properties.level == ShellUtil::CURRENT_USER)
                            ? fake_start_menu_.GetPath()
                            : fake_common_start_menu_.GetPath();
        break;
      case ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED:
        expected_path = (properties.level == ShellUtil::CURRENT_USER)
                            ? fake_start_menu_.GetPath()
                            : fake_common_start_menu_.GetPath();
        expected_path = expected_path.Append(
            InstallUtil::GetChromeShortcutDirNameDeprecated());
        break;
      default:
        ADD_FAILURE() << "Unknown location";
        return base::FilePath();
    }

    std::wstring shortcut_name = properties.has_shortcut_name()
                                     ? properties.shortcut_name
                                     : InstallUtil::GetShortcutName();
    shortcut_name += installer::kLnkExt;
    return expected_path.Append(shortcut_name);
  }

  // Validates that the shortcut at |location| matches |properties| (and
  // implicit default properties) for |dist|.
  // Note: This method doesn't verify the |pin_to_taskbar| property as it
  // implies real (non-mocked) state which is flaky to test.
  void ValidateChromeShortcut(ShellUtil::ShortcutLocation location,
                              const ShellUtil::ShortcutProperties& properties) {
    base::FilePath expected_path(GetExpectedShortcutPath(location, properties));

    base::win::ShortcutProperties expected_properties;
    if (properties.has_target()) {
      expected_properties.set_target(properties.target);
      expected_properties.set_working_dir(properties.target.DirName());
    } else {
      expected_properties.set_target(chrome_exe_);
      expected_properties.set_working_dir(chrome_exe_.DirName());
    }

    if (properties.has_arguments())
      expected_properties.set_arguments(properties.arguments);
    else
      expected_properties.set_arguments(std::wstring());

    if (properties.has_description())
      expected_properties.set_description(properties.description);
    else
      expected_properties.set_description(InstallUtil::GetAppDescription());

    if (properties.has_icon()) {
      expected_properties.set_icon(properties.icon, properties.icon_index);
    } else {
      int icon_index = install_static::GetAppIconResourceIndex();
      expected_properties.set_icon(chrome_exe_, icon_index);
    }

    if (properties.has_app_id()) {
      expected_properties.set_app_id(properties.app_id);
    } else {
      // Tests are always seen as user-level installs in ShellUtil.
      expected_properties.set_app_id(ShellUtil::GetBrowserModelId(true));
    }

    if (properties.has_toast_activator_clsid()) {
      expected_properties.set_toast_activator_clsid(
          properties.toast_activator_clsid);
    } else {
      expected_properties.set_toast_activator_clsid(CLSID_NULL);
    }

    base::win::ValidateShortcut(expected_path, expected_properties);
  }

  // A ShellUtil::ShortcutProperties object with common properties set already.
  ShellUtil::ShortcutProperties test_properties_;

  base::ScopedTempDir temp_dir_;
  base::ScopedTempDir fake_user_desktop_;
  base::ScopedTempDir fake_common_desktop_;
  base::ScopedTempDir fake_user_quick_launch_;
  base::ScopedTempDir fake_default_user_quick_launch_;
  base::ScopedTempDir fake_start_menu_;
  base::ScopedTempDir fake_common_start_menu_;
  base::ScopedTempDir fake_user_startup_;
  base::ScopedTempDir fake_common_startup_;
  std::unique_ptr<base::ScopedPathOverride> user_desktop_override_;
  std::unique_ptr<base::ScopedPathOverride> common_desktop_override_;
  std::unique_ptr<base::ScopedPathOverride> user_quick_launch_override_;
  std::unique_ptr<base::ScopedPathOverride> start_menu_override_;
  std::unique_ptr<base::ScopedPathOverride> common_start_menu_override_;
  std::unique_ptr<base::ScopedPathOverride> user_startup_override_;
  std::unique_ptr<base::ScopedPathOverride> common_startup_override_;

  base::FilePath chrome_exe_;
  base::FilePath chrome_proxy_exe_;
  base::FilePath manganese_exe_;
  base::FilePath iron_exe_;
  base::FilePath other_ico_;
};

}  // namespace

TEST_F(ShellUtilShortcutTest, GetShortcutPath) {
  base::FilePath path;

  ASSERT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                         ShellUtil::CURRENT_USER, &path));
  EXPECT_EQ(fake_user_desktop_.GetPath(), path);

  ASSERT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                         ShellUtil::SYSTEM_LEVEL, &path));
  EXPECT_EQ(fake_common_desktop_.GetPath(), path);

  ASSERT_TRUE(
      ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                                 ShellUtil::CURRENT_USER, &path));
  EXPECT_EQ(fake_user_quick_launch_.GetPath(), path);

  std::wstring start_menu_subfolder =
      InstallUtil::GetChromeShortcutDirNameDeprecated();
  ASSERT_TRUE(ShellUtil::GetShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      ShellUtil::CURRENT_USER, &path));
  EXPECT_EQ(fake_start_menu_.GetPath().Append(start_menu_subfolder), path);

  ASSERT_TRUE(ShellUtil::GetShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      ShellUtil::SYSTEM_LEVEL, &path));
  EXPECT_EQ(fake_common_start_menu_.GetPath().Append(start_menu_subfolder),
            path);

  ASSERT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_STARTUP,
                                         ShellUtil::SYSTEM_LEVEL, &path));
  EXPECT_EQ(fake_common_startup_.GetPath(), path);

  ASSERT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_STARTUP,
                                         ShellUtil::CURRENT_USER, &path));
  EXPECT_EQ(fake_user_startup_.GetPath(), path);
}

TEST_F(ShellUtilShortcutTest, MoveExistingShortcut) {
  test_properties_.set_shortcut_name(L"Bobo le shortcut");
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  base::FilePath old_shortcut_path(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_));

  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_);
  ASSERT_TRUE(base::PathExists(old_shortcut_path.DirName()));
  ASSERT_TRUE(base::PathExists(old_shortcut_path));

  ASSERT_TRUE(ShellUtil::MoveExistingShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, test_properties_));

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT,
                         test_properties_);
  ASSERT_FALSE(base::PathExists(old_shortcut_path));
  ASSERT_FALSE(base::PathExists(old_shortcut_path.DirName()));
}

// Test the basic mechanism of TranslateShortcutCreationOrUpdateInfo.
// Other tests that call ShellUtil::CreateOrUpdateShortcut exercise its
// complete functionality.
TEST_F(ShellUtilShortcutTest, TranslateShortcutCreateOrUpdateInfo) {
  ShellUtil::ShortcutLocation location = ShellUtil::SHORTCUT_LOCATION_DESKTOP;
  test_properties_.set_target(chrome_exe_);
  ShellUtil::ShortcutOperation operation =
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS;
  base::win::ShortcutOperation base_operation;
  base::win::ShortcutProperties base_properties;
  base::FilePath base_shortcut_path;
  bool should_install_shortcut = false;
  EXPECT_TRUE(ShellUtil::TranslateShortcutCreationOrUpdateInfo(
      location, test_properties_, operation, base_operation, base_properties,
      should_install_shortcut, base_shortcut_path));
  EXPECT_EQ(base_operation, base::win::ShortcutOperation::kCreateAlways);
  EXPECT_EQ(base_properties.target, chrome_exe_);
  EXPECT_TRUE(should_install_shortcut);
  EXPECT_EQ(base_shortcut_path,
            GetExpectedShortcutPath(location, test_properties_));
}

TEST_F(ShellUtilShortcutTest, CreateChromeExeShortcutWithDefaultProperties) {
  ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
  ShellUtil::AddDefaultShortcutProperties(chrome_exe_, &properties);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, properties,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP, properties);
}

TEST_F(ShellUtilShortcutTest, CreateStartMenuShortcutWithAllProperties) {
  test_properties_.set_shortcut_name(L"Bobo le shortcut");
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ValidateChromeShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_);
}

TEST_F(ShellUtilShortcutTest, ReplaceSystemLevelDesktopShortcut) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  ShellUtil::ShortcutProperties new_properties(ShellUtil::SYSTEM_LEVEL);
  ShellUtil::AddDefaultShortcutProperties(chrome_exe_, &new_properties);
  new_properties.set_description(L"New description");
  new_properties.set_arguments(L"--new-arguments");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, new_properties,
      ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING));

  // Expect the properties set in |new_properties| to be set as above and
  // properties that don't have a default value to be set back to their default
  // (as validated in ValidateChromeShortcut()) or unset if they don't .
  ShellUtil::ShortcutProperties expected_properties(new_properties);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, UpdateQuickLaunchShortcutArguments) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  // Only changing one property, don't need all the defaults.
  ShellUtil::ShortcutProperties updated_properties(ShellUtil::CURRENT_USER);
  updated_properties.set_arguments(L"--updated --arguments");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH, updated_properties,
      ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING));

  // Expect the properties set in |updated_properties| to be set as above and
  // all other properties to remain unchanged.
  ShellUtil::ShortcutProperties expected_properties(test_properties_);
  expected_properties.set_arguments(updated_properties.arguments);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevel) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         test_properties_);
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevelWithSystemLevelPresent) {
  std::wstring shortcut_name(InstallUtil::GetShortcutName() +
                             installer::kLnkExt);

  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(
      base::PathExists(fake_common_desktop_.GetPath().Append(shortcut_name)));

  test_properties_.level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ASSERT_FALSE(
      base::PathExists(fake_user_desktop_.GetPath().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, CreateIfNoSystemLevelStartMenu) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_, ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL));
  ValidateChromeShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_);
}

TEST_F(ShellUtilShortcutTest, CreateAlwaysUserWithSystemLevelPresent) {
  std::wstring shortcut_name(InstallUtil::GetShortcutName() +
                             installer::kLnkExt);

  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(
      base::PathExists(fake_common_desktop_.GetPath().Append(shortcut_name)));

  test_properties_.level = ShellUtil::CURRENT_USER;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(
      base::PathExists(fake_user_desktop_.GetPath().Append(shortcut_name)));
}

TEST_F(ShellUtilShortcutTest, RemoveChromeShortcut) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcuts(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                         ShellUtil::CURRENT_USER,
                                         {chrome_exe_}));
  ASSERT_FALSE(base::PathExists(shortcut_path));
  ASSERT_TRUE(base::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveSystemLevelChromeShortcut) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut_path));

  ASSERT_TRUE(ShellUtil::RemoveShortcuts(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                         ShellUtil::SYSTEM_LEVEL,
                                         {chrome_exe_}));
  ASSERT_FALSE(base::PathExists(shortcut_path));
  ASSERT_TRUE(base::PathExists(shortcut_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveMultipleChromeShortcuts) {
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));

  // Shortcut 2: targets "chrome.exe"; has arguments; icon set to "other.ico".
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  test_properties_.set_icon(other_ico_, 0);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));

  // Shortcut 3: targets "iron.exe"; has arguments; icon set to "chrome.exe".
  test_properties_.set_shortcut_name(L"Iron 3");
  test_properties_.set_target(iron_exe_);
  test_properties_.set_icon(chrome_exe_, 3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));

  // Remove shortcuts that target "chrome.exe".
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                         ShellUtil::CURRENT_USER,
                                         {chrome_exe_}));
  ASSERT_FALSE(base::PathExists(shortcut1_path));
  ASSERT_FALSE(base::PathExists(shortcut2_path));
  ASSERT_TRUE(base::PathExists(shortcut3_path));
  ASSERT_TRUE(base::PathExists(shortcut1_path.DirName()));
}

TEST_F(ShellUtilShortcutTest, RemoveMultiTargetShortcuts) {
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));

  // Shortcut 2: targets "chrome_proxy.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome Proxy 2");
  test_properties_.set_target(chrome_proxy_exe_);
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));

  // Shortcut 3: targets "chrome_proxy.exe"; has arguments; icon set to
  // "other.ico".
  test_properties_.set_shortcut_name(L"Chrome Proxy 3");
  test_properties_.set_target(chrome_proxy_exe_);
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  test_properties_.set_icon(other_ico_, 0);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));

  // Shortcut 4: targets "iron.exe"; has arguments; icon set to "chrome.exe".
  test_properties_.set_shortcut_name(L"Iron 4");
  test_properties_.set_target(iron_exe_);
  test_properties_.set_icon(chrome_exe_, 3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut4_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut4_path));

  // Shortcut 5: targets "manganese.exe"; has arguments; icon set to
  // "chrome.exe".
  test_properties_.set_shortcut_name(L"Manganese 5");
  test_properties_.set_target(manganese_exe_);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut5_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut5_path));

  // Remove per-user shortcuts that target "chrome.exe", "chrome_proxy.exe" and
  // "iron.exe".
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::CURRENT_USER,
      {chrome_exe_, chrome_proxy_exe_, iron_exe_}));
  ASSERT_FALSE(base::PathExists(shortcut1_path));
  ASSERT_FALSE(base::PathExists(shortcut2_path));
  ASSERT_FALSE(base::PathExists(shortcut3_path));
  ASSERT_FALSE(base::PathExists(shortcut4_path));
  ASSERT_TRUE(base::PathExists(shortcut5_path));
}

TEST_F(ShellUtilShortcutTest, RemoveSystemLevelMultiTargetShortcuts) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));

  // Shortcut 2: targets "chrome_proxy.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome Proxy 2");
  test_properties_.set_target(chrome_proxy_exe_);
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));

  // Shortcut 3: targets "chrome_proxy.exe"; has arguments; icon set to
  // "other.ico".
  test_properties_.set_shortcut_name(L"Chrome Proxy 3");
  test_properties_.set_target(chrome_proxy_exe_);
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  test_properties_.set_icon(other_ico_, 0);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));

  // Shortcut 4: targets "iron.exe"; has arguments; icon set to "chrome.exe".
  test_properties_.set_shortcut_name(L"Iron 4");
  test_properties_.set_target(iron_exe_);
  test_properties_.set_icon(chrome_exe_, 3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut4_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut4_path));

  // Shortcut 5: targets "manganese.exe"; has arguments; icon set to
  // "chrome.exe".
  test_properties_.set_shortcut_name(L"Manganese 5");
  test_properties_.set_target(manganese_exe_);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut5_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut5_path));

  // Remove system level shortcuts that target "chrome.exe", "chrome_proxy.exe"
  // and "iron.exe".
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::SYSTEM_LEVEL,
      {chrome_exe_, chrome_proxy_exe_, iron_exe_}));
  ASSERT_FALSE(base::PathExists(shortcut1_path));
  ASSERT_FALSE(base::PathExists(shortcut2_path));
  ASSERT_FALSE(base::PathExists(shortcut3_path));
  ASSERT_FALSE(base::PathExists(shortcut4_path));
  ASSERT_TRUE(base::PathExists(shortcut5_path));
}

TEST_F(ShellUtilShortcutTest, RemoveAllShortcutsMultipleLocations) {
  // Shortcut 1: targets "chrome.exe"; desktop shortcut.
  test_properties_.set_shortcut_name(L"Chrome Desktop 1");
  test_properties_.set_target(chrome_exe_);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));

  // Shortcut 2: targets "chrome_proxy.exe"; start menu shortcut.
  test_properties_.set_shortcut_name(L"Chrome Proxy Startup 2");
  test_properties_.set_target(chrome_proxy_exe_);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));

  // Shortcut 3: targets "iron.exe"; has arguments; icon set to "chrome.exe".
  test_properties_.set_shortcut_name(L"Iron 3");
  test_properties_.set_target(iron_exe_);
  test_properties_.set_icon(chrome_exe_, 3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));

  ShellUtil::RemoveAllShortcuts(ShellUtil::CURRENT_USER,
                                {chrome_exe_, chrome_proxy_exe_});
  ASSERT_FALSE(base::PathExists(shortcut1_path));
  ASSERT_FALSE(base::PathExists(shortcut2_path));
  ASSERT_TRUE(base::PathExists(shortcut3_path));
}

TEST_F(ShellUtilShortcutTest, RemoveAllShortcutsSystemLevelMultipleLocations) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  // Shortcut 1: targets "chrome.exe"; desktop shortcut.
  test_properties_.set_shortcut_name(L"Chrome Desktop 1");
  test_properties_.set_target(chrome_exe_);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));

  // Shortcut 2: targets "chrome_proxy.exe"; start menu shortcut.
  test_properties_.set_shortcut_name(L"Chrome Proxy Startup 2");
  test_properties_.set_target(chrome_proxy_exe_);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));

  // Shortcut 3: targets "iron.exe"; has arguments; icon set to "chrome.exe".
  test_properties_.set_shortcut_name(L"Iron 3");
  test_properties_.set_target(iron_exe_);
  test_properties_.set_icon(chrome_exe_, 3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));

  ShellUtil::RemoveAllShortcuts(ShellUtil::SYSTEM_LEVEL,
                                {chrome_exe_, chrome_proxy_exe_});
  ASSERT_FALSE(base::PathExists(shortcut1_path));
  ASSERT_FALSE(base::PathExists(shortcut2_path));
  ASSERT_TRUE(base::PathExists(shortcut3_path));
}

TEST_F(ShellUtilShortcutTest, RetargetShortcutsWithArgs) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_)));

  base::FilePath new_exe = manganese_exe_;
  // Relies on the fact that |test_properties_| has non-empty arguments.
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::CURRENT_USER,
      chrome_exe_, new_exe));

  ShellUtil::ShortcutProperties expected_properties(test_properties_);
  expected_properties.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, RetargetSystemLevelChromeShortcutsWithArgs) {
  test_properties_.level = ShellUtil::SYSTEM_LEVEL;
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_)));

  base::FilePath new_exe = manganese_exe_;
  // Relies on the fact that |test_properties_| has non-empty arguments.
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::SYSTEM_LEVEL,
      chrome_exe_, new_exe));

  ShellUtil::ShortcutProperties expected_properties(test_properties_);
  expected_properties.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties);
}

TEST_F(ShellUtilShortcutTest, RetargetChromeShortcutsWithArgsEmpty) {
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties1(test_properties_);

  // Shortcut 2: targets "chrome.exe"; has arguments.
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties2(test_properties_);

  // Retarget shortcuts: replace "chrome.exe" with "manganese.exe". Only
  // shortcuts with non-empty arguments (i.e., shortcut 2) gets updated.
  base::FilePath new_exe = manganese_exe_;
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::CURRENT_USER,
      chrome_exe_, new_exe));

  // Verify shortcut 1: no change.
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties1);

  // Verify shortcut 2: target => "manganese.exe".
  expected_properties2.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties2);
}

TEST_F(ShellUtilShortcutTest, RetargetChromeShortcutsWithArgsIcon) {
  const int kTestIconIndex1 = 3;
  const int kTestIconIndex2 = 5;
  const int kTestIconIndex3 = 8;

  // Shortcut 1: targets "chrome.exe"; icon same as target.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_icon(test_properties_.target, kTestIconIndex1);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties1(test_properties_);

  // Shortcut 2: targets "chrome.exe"; icon set to "other.ico".
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_icon(other_ico_, kTestIconIndex2);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties2(test_properties_);

  // Shortcut 3: targets "iron.exe"; icon set to "chrome.exe".
  test_properties_.set_target(iron_exe_);
  test_properties_.set_shortcut_name(L"Iron 3");
  test_properties_.set_icon(chrome_exe_, kTestIconIndex3);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(base::PathExists(GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_)));
  ShellUtil::ShortcutProperties expected_properties3(test_properties_);

  // Retarget shortcuts: replace "chrome.exe" with "manganese.exe".
  // Relies on the fact that |test_properties_| has non-empty arguments.
  base::FilePath new_exe = manganese_exe_;
  ASSERT_TRUE(ShellUtil::RetargetShortcutsWithArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::CURRENT_USER,
      chrome_exe_, new_exe));

  // Verify shortcut 1: target & icon => "manganese.exe", kept same icon index.
  expected_properties1.set_target(new_exe);
  expected_properties1.set_icon(new_exe, kTestIconIndex1);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties1);

  // Verify shortcut 2: target => "manganese.exe", kept icon.
  expected_properties2.set_target(new_exe);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties2);

  // Verify shortcut 3: no change, since target doesn't match.
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties3);
}

TEST_F(ShellUtilShortcutTest, ClearShortcutArguments) {
  // Shortcut 1: targets "chrome.exe"; no arguments.
  test_properties_.set_shortcut_name(L"Chrome 1");
  test_properties_.set_arguments(L"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut1_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut1_path));
  ShellUtil::ShortcutProperties expected_properties1(test_properties_);

  // Shortcut 2: targets "chrome.exe"; has 1 argument in the allow list.
  test_properties_.set_shortcut_name(L"Chrome 2");
  test_properties_.set_arguments(L"--profile-directory=\"Profile 2\"");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut2_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut2_path));
  ShellUtil::ShortcutProperties expected_properties2(test_properties_);

  // Shortcut 3: targets "chrome.exe"; has an unknown argument.
  test_properties_.set_shortcut_name(L"Chrome 3");
  test_properties_.set_arguments(L"foo.com");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut3_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut3_path));
  ShellUtil::ShortcutProperties expected_properties3(test_properties_);

  // Shortcut 4: targets "chrome.exe"; has both unknown and known arguments.
  const std::wstring kKnownArg = L"--app-id";
  const std::wstring kExpectedArgs = L"foo.com " + kKnownArg;
  test_properties_.set_shortcut_name(L"Chrome 4");
  test_properties_.set_arguments(kExpectedArgs);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath shortcut4_path = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(shortcut4_path));
  ShellUtil::ShortcutProperties expected_properties4(test_properties_);

  // List the shortcuts.
  std::vector<std::pair<base::FilePath, std::wstring>> shortcuts;
  EXPECT_TRUE(ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::CURRENT_USER,
      chrome_exe_, false, nullptr, &shortcuts));
  ASSERT_EQ(2u, shortcuts.size());
  std::pair<base::FilePath, std::wstring> shortcut3 =
      shortcuts[0].first == shortcut3_path ? shortcuts[0] : shortcuts[1];
  std::pair<base::FilePath, std::wstring> shortcut4 =
      shortcuts[0].first == shortcut4_path ? shortcuts[0] : shortcuts[1];
  EXPECT_EQ(shortcut3_path, shortcut3.first);
  EXPECT_EQ(L"foo.com", shortcut3.second);
  EXPECT_EQ(shortcut4_path, shortcut4.first);
  EXPECT_EQ(kExpectedArgs, shortcut4.second);

  // Clear shortcuts.
  shortcuts.clear();
  EXPECT_TRUE(ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::CURRENT_USER,
      chrome_exe_, true, nullptr, &shortcuts));
  ASSERT_EQ(2u, shortcuts.size());
  shortcut3 =
      shortcuts[0].first == shortcut3_path ? shortcuts[0] : shortcuts[1];
  shortcut4 =
      shortcuts[0].first == shortcut4_path ? shortcuts[0] : shortcuts[1];
  EXPECT_EQ(shortcut3_path, shortcut3.first);
  EXPECT_EQ(L"foo.com", shortcut3.second);
  EXPECT_EQ(shortcut4_path, shortcut4.first);
  EXPECT_EQ(kExpectedArgs, shortcut4.second);

  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties1);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties2);
  expected_properties3.set_arguments(std::wstring());
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties3);
  expected_properties4.set_arguments(kKnownArg);
  ValidateChromeShortcut(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                         expected_properties4);
}

TEST_F(ShellUtilShortcutTest, ShortcutsAreNotHidden) {
  // Shortcut 1: targets "chrome.exe"; not hidden.
  test_properties_.set_shortcut_name(L"Chrome Visible");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath visible_shortcut = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(visible_shortcut));

  // Shortcut 2: targets "chrome.exe"; hidden.
  test_properties_.set_shortcut_name(L"Chrome Hidden");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  base::FilePath hidden_shortcut = GetExpectedShortcutPath(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_);
  ASSERT_TRUE(base::PathExists(hidden_shortcut));
  DWORD shortcut_attributes =
      ::GetFileAttributes(hidden_shortcut.value().c_str());
  ASSERT_NE(shortcut_attributes, INVALID_FILE_ATTRIBUTES);
  ASSERT_TRUE(::SetFileAttributes(hidden_shortcut.value().c_str(),
                                  shortcut_attributes | FILE_ATTRIBUTE_HIDDEN));

  EXPECT_TRUE(ShellUtil::ResetShortcutFileAttributes(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, ShellUtil::CURRENT_USER,
      chrome_exe_));

  shortcut_attributes = ::GetFileAttributes(visible_shortcut.value().c_str());
  EXPECT_EQ(shortcut_attributes & FILE_ATTRIBUTE_HIDDEN, 0UL);
  shortcut_attributes = ::GetFileAttributes(hidden_shortcut.value().c_str());
  EXPECT_EQ(shortcut_attributes & FILE_ATTRIBUTE_HIDDEN, 0UL);
}

TEST_F(ShellUtilShortcutTest, CreateMultipleStartMenuShortcutsAndRemoveFolder) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  test_properties_.set_shortcut_name(L"A second shortcut");
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      test_properties_, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  base::FilePath chrome_shortcut_folder(fake_start_menu_.GetPath().Append(
      InstallUtil::GetChromeShortcutDirNameDeprecated()));
  base::FilePath chrome_apps_shortcut_folder(fake_start_menu_.GetPath().Append(
      InstallUtil::GetChromeAppsShortcutDirName()));

  base::FileEnumerator chrome_file_counter(chrome_shortcut_folder, false,
                                           base::FileEnumerator::FILES);
  int count = 0;
  while (!chrome_file_counter.Next().empty())
    ++count;
  EXPECT_EQ(2, count);

  base::FileEnumerator chrome_apps_file_counter(
      chrome_apps_shortcut_folder, false, base::FileEnumerator::FILES);
  count = 0;
  while (!chrome_apps_file_counter.Next().empty())
    ++count;
  EXPECT_EQ(2, count);

  ASSERT_TRUE(base::PathExists(chrome_shortcut_folder));
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
      ShellUtil::CURRENT_USER, {chrome_exe_}));
  ASSERT_FALSE(base::PathExists(chrome_shortcut_folder));

  ASSERT_TRUE(base::PathExists(chrome_apps_shortcut_folder));
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
      ShellUtil::CURRENT_USER, {chrome_exe_}));
  ASSERT_FALSE(base::PathExists(chrome_apps_shortcut_folder));
}

TEST_F(ShellUtilShortcutTest,
       DeleteStartMenuRootShortcutWithoutRemovingFolder) {
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  std::wstring shortcut_name(InstallUtil::GetShortcutName() +
                             installer::kLnkExt);
  base::FilePath shortcut_path(
      fake_start_menu_.GetPath().Append(shortcut_name));

  ASSERT_TRUE(base::PathExists(fake_start_menu_.GetPath()));
  ASSERT_TRUE(base::PathExists(shortcut_path));
  ASSERT_TRUE(
      ShellUtil::RemoveShortcuts(ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT,
                                 ShellUtil::CURRENT_USER, {chrome_exe_}));
  // The shortcut should be removed but the "Start Menu" root directory should
  // remain.
  ASSERT_TRUE(base::PathExists(fake_start_menu_.GetPath()));
  ASSERT_FALSE(base::PathExists(shortcut_path));
}

TEST_F(ShellUtilShortcutTest, DontRemoveChromeShortcutIfPointsToAnotherChrome) {
  base::ScopedTempDir other_exe_dir;
  ASSERT_TRUE(other_exe_dir.CreateUniqueTempDir());
  base::FilePath other_chrome_exe =
      other_exe_dir.GetPath().Append(installer::kChromeExe);
  EXPECT_TRUE(base::WriteFile(other_chrome_exe, ""));

  test_properties_.set_target(other_chrome_exe);
  ASSERT_TRUE(ShellUtil::CreateOrUpdateShortcut(
      ShellUtil::SHORTCUT_LOCATION_DESKTOP, test_properties_,
      ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS));

  std::wstring shortcut_name(InstallUtil::GetShortcutName() +
                             installer::kLnkExt);
  base::FilePath shortcut_path(
      fake_user_desktop_.GetPath().Append(shortcut_name));
  ASSERT_TRUE(base::PathExists(shortcut_path));

  // The shortcut shouldn't be removed as it was installed pointing to
  // |other_chrome_exe| and RemoveChromeShortcut() is being told that the
  // removed shortcut should point to |chrome_exe_|.
  ASSERT_TRUE(ShellUtil::RemoveShortcuts(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                         ShellUtil::CURRENT_USER,
                                         {chrome_exe_}));
  ASSERT_TRUE(base::PathExists(shortcut_path));
  ASSERT_TRUE(base::PathExists(shortcut_path.DirName()));
}

class ShellUtilRegistryTest : public testing::Test {
 public:
  ShellUtilRegistryTest() = default;
  ShellUtilRegistryTest(const ShellUtilRegistryTest&) = delete;
  ShellUtilRegistryTest& operator=(const ShellUtilRegistryTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER));

    // .test2 files already have a default application.
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, L"Software\\Classes\\.test2",
                         KEY_ALL_ACCESS));
    EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"", L"SomeOtherApp"));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    chrome_exe_ = temp_dir_.GetPath().Append(installer::kChromeExe);
  }

  static base::CommandLine OpenCommand() {
    base::FilePath open_command_path(kTestOpenCommand);
    base::CommandLine open_command(open_command_path);
    return open_command;
  }

  static const std::set<std::wstring> FileExtensions() {
    std::set<std::wstring> file_extensions;
    for (size_t i = 0; i < std::size(kTestFileExtensions); ++i)
      file_extensions.insert(kTestFileExtensions[i]);
    return file_extensions;
  }

  const base::FilePath& chrome_exe() const { return chrome_exe_; }

 private:
  registry_util::RegistryOverrideManager registry_overrides_;
  base::ScopedTempDir temp_dir_;
  base::FilePath chrome_exe_;
};

TEST_F(ShellUtilRegistryTest, AddFileAssociations) {
  // Create file associations.
  EXPECT_TRUE(ShellUtil::AddFileAssociations(
      kTestProgId, OpenCommand(), kTestApplicationName, kTestFileTypeName,
      base::FilePath(kTestIconPath), FileExtensions()));

  // Ensure that the registry keys have been correctly set.
  base::win::RegKey key;
  std::wstring value;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\TestApp", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"Test File Type", value);
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"FileExtensions", &value));
  EXPECT_EQ(L".test1;.test2", value);
  ASSERT_EQ(
      ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER,
               L"Software\\Classes\\TestApp\\shell\\open\\command", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"\"C:\\test.exe\" --single-argument %1", value);

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER,
                     L"Software\\Classes\\TestApp\\Application", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"ApplicationName", &value));
  EXPECT_EQ(L"Test Application", value);
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"ApplicationIcon", &value));
  EXPECT_EQ(L"D:\\test.ico,0", value);

  // .test1 should not be default-associated with our test app. Programmatically
  // becoming the default handler can be surprising to users, and risks
  // overwriting affected file types' implicit default handlers, which are
  // cached by Windows.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\.test1", KEY_READ));

  // .test 1 should have our app in its Open With list.
  EXPECT_NE(ERROR_SUCCESS, key.ReadValue(L"", &value));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER,
                     L"Software\\Classes\\.test1\\OpenWithProgids", KEY_READ));
  EXPECT_TRUE(key.HasValue(L"TestApp"));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"TestApp", &value));
  EXPECT_EQ(L"", value);

  // .test2 should still be associated with the other app (should not have been
  // overridden). But it should have our app in its Open With list.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\.test2", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"SomeOtherApp", value);
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER,
                     L"Software\\Classes\\.test2\\OpenWithProgids", KEY_READ));
  EXPECT_TRUE(key.HasValue(L"TestApp"));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"TestApp", &value));
  EXPECT_EQ(L"", value);
}

TEST_F(ShellUtilRegistryTest, DeleteFileAssociations) {
  // Create file associations.
  ASSERT_TRUE(ShellUtil::AddFileAssociations(
      kTestProgId, OpenCommand(), kTestApplicationName, kTestFileTypeName,
      base::FilePath(kTestIconPath), FileExtensions()));

  // Delete them.
  EXPECT_TRUE(ShellUtil::DeleteFileAssociations(kTestProgId));

  // The class key should have been completely deleted.
  base::win::RegKey key;
  std::wstring value;
  ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\TestApp", KEY_READ));

  // .test1 and .test2 should no longer be associated with the test app.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER,
                     L"Software\\Classes\\.test1\\OpenWithProgids", KEY_READ));
  EXPECT_FALSE(key.HasValue(L"TestApp"));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER,
                     L"Software\\Classes\\.test2\\OpenWithProgids", KEY_READ));
  EXPECT_FALSE(key.HasValue(L"TestApp"));

  // .test2 should still have the other app as its default handler.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\.test2", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"SomeOtherApp", value);
}

TEST_F(ShellUtilRegistryTest, RegisterFileHandlerProgIds) {
  const std::vector<std::wstring> file_handler_prog_ids(
      {std::wstring(kFileHandler1ProgId), std::wstring(kFileHandler2ProgId)});
  ShellUtil::RegisterFileHandlerProgIdsForAppId(std::wstring(kTestProgId),
                                                file_handler_prog_ids);
  // Test that the registry contains the file handler prog ids.
  const std::vector<std::wstring> retrieved_file_handler_prog_ids =
      ShellUtil::GetFileHandlerProgIdsForAppId(std::wstring(kTestProgId));
  EXPECT_EQ(file_handler_prog_ids, retrieved_file_handler_prog_ids);
}

TEST_F(ShellUtilRegistryTest, AddApplicationClass) {
  // Add TestApp application class and verify registry entries.
  EXPECT_TRUE(ShellUtil::AddApplicationClass(
      std::wstring(kTestProgId), OpenCommand(), kTestApplicationName,
      kTestFileTypeName, base::FilePath(kTestIconPath)));

  base::win::RegKey key;
  std::wstring value;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\TestApp", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"Test File Type", value);
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER,
                     L"Software\\Classes\\TestApp\\DefaultIcon", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"D:\\test.ico,0", value);
  ASSERT_EQ(
      ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER,
               L"Software\\Classes\\TestApp\\shell\\open\\command", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"", &value));
  EXPECT_EQ(L"\"C:\\test.exe\" --single-argument %1", value);

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER,
                     L"Software\\Classes\\TestApp\\Application", KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"ApplicationName", &value));
  EXPECT_EQ(L"Test Application", value);
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(L"ApplicationIcon", &value));
  EXPECT_EQ(L"D:\\test.ico,0", value);
}

TEST_F(ShellUtilRegistryTest, DeleteApplicationClass) {
  ASSERT_TRUE(ShellUtil::AddApplicationClass(
      kTestProgId, OpenCommand(), kTestApplicationName, kTestFileTypeName,
      base::FilePath(kTestIconPath)));

  base::win::RegKey key;
  std::wstring value;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\TestApp", KEY_READ));

  EXPECT_TRUE(ShellUtil::DeleteApplicationClass(kTestProgId));
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\TestApp", KEY_READ));
}

TEST_F(ShellUtilRegistryTest, GetAppName) {
  const std::wstring empty_app_name(ShellUtil::GetAppName(kTestProgId));
  EXPECT_TRUE(empty_app_name.empty());

  // Add file associations and test that GetAppName returns the registered app
  // name. Pass kTestApplicationName for the open command, to handle the Win 7
  // case, which returns the open command executable name as the app_name.
  ASSERT_TRUE(ShellUtil::AddFileAssociations(
      kTestProgId, OpenCommand(), kTestApplicationName, kTestFileTypeName,
      base::FilePath(kTestIconPath), FileExtensions()));
  const std::wstring app_name(ShellUtil::GetAppName(kTestProgId));
  EXPECT_EQ(app_name, kTestApplicationName);
}

TEST_F(ShellUtilRegistryTest, GetApplicationInfoForProgId) {
  ShellUtil::ApplicationInfo empty_application_info(
      ShellUtil::GetApplicationInfoForProgId(kTestProgId));
  EXPECT_TRUE(empty_application_info.application_name.empty());

  // Add application class and test that GetApplicationInfoForProgId returns
  // the registered application properties.
  EXPECT_TRUE(ShellUtil::AddApplicationClass(
      std::wstring(kTestProgId), OpenCommand(), kTestApplicationName,
      kTestApplicationDescription, base::FilePath(kTestIconPath)));

  ShellUtil::ApplicationInfo app_info(
      ShellUtil::GetApplicationInfoForProgId(kTestProgId));

  EXPECT_EQ(kTestProgId, app_info.prog_id);

  EXPECT_EQ(app_info.application_description, app_info.file_type_name);
  EXPECT_EQ(base::FilePath(kTestIconPath), app_info.file_type_icon_path);
  EXPECT_EQ(0, app_info.file_type_icon_index);

  EXPECT_EQ(L"\"C:\\test.exe\" --single-argument %1", app_info.command_line);

  EXPECT_EQ(L"", app_info.app_id);

  EXPECT_EQ(kTestApplicationName, app_info.application_name);
  EXPECT_EQ(kTestApplicationDescription, app_info.application_description);
  EXPECT_EQ(L"", app_info.publisher_name);

  EXPECT_EQ(base::FilePath(kTestIconPath), app_info.application_icon_path);
  EXPECT_EQ(0, app_info.application_icon_index);
}

TEST_F(ShellUtilRegistryTest, AddAppProtocolAssociations) {
  // Create test protocol associations.
  const std::wstring app_progid = L"app_progid1";
  const std::vector<std::wstring> app_protocols = {L"web+test", L"mailto"};

  ASSERT_TRUE(ShellUtil::AddAppProtocolAssociations(app_protocols, app_progid));

  // Ensure that classes were created for each protocol.
  // HKEY_CURRENT_USER\Software\Classes\<protocol>\URL Protocol
  base::win::RegKey key;
  std::wstring value;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\web+test", KEY_READ));
  EXPECT_TRUE(key.HasValue(L"URL Protocol"));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\mailto", KEY_READ));
  EXPECT_TRUE(key.HasValue(L"URL Protocol"));

  // Ensure that URLAssociations entries were created for each protocol.
  // "HKEY_CURRENT_USER\Software\[CompanyPathName\]ProductPathName[install_suffix]\AppProtocolHandlers\|prog_id|\Capabilities\URLAssociations\<protocol>".
  std::wstring capabilities_path(install_static::GetRegistryPath());
  capabilities_path.append(ShellUtil::kRegAppProtocolHandlers);
  capabilities_path.push_back(base::FilePath::kSeparators[0]);
  capabilities_path.append(app_progid);
  capabilities_path.append(L"\\Capabilities");

  const std::wstring url_associations_key_name =
      capabilities_path + L"\\URLAssociations";

  ASSERT_EQ(
      ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, url_associations_key_name.c_str(), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS, key.ReadValue(L"web+test", &value));
  EXPECT_EQ(app_progid, std::wstring(value));

  ASSERT_EQ(ERROR_SUCCESS, key.ReadValue(L"mailto", &value));
  EXPECT_EQ(app_progid, std::wstring(value));

  // Ensure that app was registered correctly under RegisteredApplications.
  // "HKEY_CURRENT_USER\RegisteredApplications\<prog_id>".
  ASSERT_EQ(
      ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER,
               std::wstring(ShellUtil::kRegRegisteredApplications).c_str(),
               KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS, key.ReadValue(app_progid.c_str(), &value));
  EXPECT_EQ(capabilities_path, std::wstring(value));
}

TEST_F(ShellUtilRegistryTest, ToAndFromCommandLineArgument) {
  // Create protocol associations.
  std::wstring app_progid1 = L"app_progid1";
  std::wstring app_progid2 = L"app_progid2";

  ShellUtil::ProtocolAssociations protocol_associations;
  protocol_associations.associations[L"web+test"] = app_progid1;
  protocol_associations.associations[L"mailto"] = app_progid2;

  // Ensure the above protocol_associations creates correct command line
  // arguments correctly.
  std::wstring command_line = protocol_associations.ToCommandLineArgument();
  EXPECT_EQ(L"mailto:app_progid2,web+test:app_progid1", command_line);

  // Ensure the above command line arguments parse correctly.
  std::optional<ShellUtil::ProtocolAssociations> parsed_protocol_associations =
      ShellUtil::ProtocolAssociations::FromCommandLineArgument(command_line);
  ASSERT_TRUE(parsed_protocol_associations.has_value());
  EXPECT_EQ(protocol_associations.associations,
            parsed_protocol_associations.value().associations);
  EXPECT_EQ(protocol_associations.associations[L"web+test"],
            parsed_protocol_associations.value().associations[L"web+test"]);
}

TEST_F(ShellUtilRegistryTest, RemoveAppProtocolAssociations) {
  // Create test protocol associations.
  const std::wstring app_progid = L"app_progid1";
  const std::vector<std::wstring> app_protocols = {L"web+test"};

  ASSERT_TRUE(ShellUtil::AddAppProtocolAssociations(app_protocols, app_progid));

  // Delete associations and ensure that the protocol entry does not exist.
  EXPECT_TRUE(ShellUtil::RemoveAppProtocolAssociations(app_progid));

  // Ensure that the software registration key was removed.
  // "HKEY_CURRENT_USER\Software\[CompanyPathName\]ProductPathName[install_suffix]\AppProtocolHandlers\|prog_id|".
  std::wstring capabilities_path(install_static::GetRegistryPath());
  capabilities_path.append(ShellUtil::kRegAppProtocolHandlers);
  capabilities_path.push_back(base::FilePath::kSeparators[0]);
  capabilities_path.append(app_progid);

  base::win::RegKey key;

  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_CURRENT_USER, capabilities_path.c_str(), KEY_READ));

  // Ensure that the RegisteredApplications entry was removed.
  // "HKEY_CURRENT_USER\RegisteredApplications\<prog_id>".
  ASSERT_EQ(
      ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER,
               std::wstring(ShellUtil::kRegRegisteredApplications).c_str(),
               KEY_READ));
  EXPECT_FALSE(key.HasValue(app_progid.c_str()));

  // Protocol class entry should still exist after the deleted association is
  // removed so that other associations are not affected.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    L"Software\\Classes\\web+test", KEY_READ));
  EXPECT_TRUE(key.HasValue(L"URL Protocol"));
}

TEST_F(ShellUtilRegistryTest, GetApplicationForProgId) {
  // Create file associations.
  ASSERT_TRUE(ShellUtil::AddFileAssociations(
      kTestProgId, OpenCommand(), kTestApplicationName, kTestFileTypeName,
      base::FilePath(kTestIconPath), FileExtensions()));
  base::FilePath exe_path = ShellUtil::GetApplicationPathForProgId(kTestProgId);
  EXPECT_EQ(exe_path, base::FilePath(kTestOpenCommand));
}

TEST(ShellUtilTest, BuildAppModelIdBasic) {
  std::vector<std::wstring> components;
  const std::wstring base_app_id(install_static::GetBaseAppId());
  components.push_back(base_app_id);
  ASSERT_EQ(base_app_id, ShellUtil::BuildAppUserModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdManySmall) {
  std::vector<std::wstring> components;
  const std::wstring suffixed_app_id(install_static::GetBaseAppId() +
                                     std::wstring(L".gab"));
  components.push_back(suffixed_app_id);
  components.push_back(L"Default");
  components.push_back(L"Test");
  ASSERT_EQ(suffixed_app_id + L".Default.Test",
            ShellUtil::BuildAppUserModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdNullTerminatorInTheMiddle) {
  std::vector<std::wstring> components;
  std::wstring appname_with_nullTerminator(L"I_have_null_terminator_in_middle");
  appname_with_nullTerminator[5] = '\0';
  components.push_back(appname_with_nullTerminator);
  components.push_back(L"Default");
  components.push_back(L"Test");
  std::wstring expected_string(L"I_have_nul_in_middle.Default.Test");
  expected_string[5] = '\0';
  ASSERT_EQ(expected_string, ShellUtil::BuildAppUserModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdLongUsernameNormalProfile) {
  std::vector<std::wstring> components;
  const std::wstring long_appname(
      L"Chrome.a_user_who_has_a_crazy_long_name_with_some_weird@symbols_in_it_"
      L"that_goes_over_64_characters");
  components.push_back(long_appname);
  components.push_back(L"Default");
  ASSERT_EQ(L"Chrome.a_user_wer_64_characters.Default",
            ShellUtil::BuildAppUserModelId(components));
}

TEST(ShellUtilTest, BuildAppModelIdLongEverything) {
  std::vector<std::wstring> components;
  const std::wstring long_appname(
      L"Chrome.a_user_who_has_a_crazy_long_name_with_some_weird@symbols_in_it_"
      L"that_goes_over_64_characters");
  components.push_back(long_appname);
  components.push_back(
      L"A_crazy_profile_name_not_even_sure_whether_that_is_possible");
  const std::wstring constructed_app_id(
      ShellUtil::BuildAppUserModelId(components));
  ASSERT_LE(constructed_app_id.length(), installer::kMaxAppModelIdLength);
  ASSERT_EQ(L"Chrome.a_user_wer_64_characters.A_crazy_profilethat_is_possible",
            constructed_app_id);
}

TEST(ShellUtilTest, GetUserSpecificRegistrySuffix) {
  std::wstring suffix;
  ASSERT_TRUE(ShellUtil::GetUserSpecificRegistrySuffix(&suffix));
  ASSERT_TRUE(base::StartsWith(suffix, L".", base::CompareCase::SENSITIVE));
  ASSERT_EQ(27u, suffix.length());
  ASSERT_TRUE(base::ContainsOnlyChars(suffix.substr(1),
                                      L"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"));
}

TEST(ShellUtilTest, GetOldUserSpecificRegistrySuffix) {
  std::wstring suffix;
  ASSERT_TRUE(ShellUtil::GetOldUserSpecificRegistrySuffix(&suffix));
  ASSERT_TRUE(base::StartsWith(suffix, L".", base::CompareCase::SENSITIVE));

  wchar_t user_name[256];
  DWORD size = std::size(user_name);
  ASSERT_NE(0, ::GetUserName(user_name, &size));
  ASSERT_GE(size, 1U);
  ASSERT_STREQ(user_name, suffix.substr(1).c_str());
}
