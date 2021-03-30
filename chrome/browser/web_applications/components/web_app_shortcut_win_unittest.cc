// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"

namespace web_app {
namespace internals {
namespace {

bool CreateTestAppShortcut(const base::FilePath& shortcut_path,
                           const base::FilePath::StringType& profile_name) {
  base::CommandLine args_cl(base::CommandLine::NO_PROGRAM);
  args_cl.AppendSwitchNative(switches::kProfileDirectory, profile_name);
  args_cl.AppendSwitchASCII(switches::kAppId, "123");

  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_arguments(args_cl.GetArgumentsString());
  shortcut_properties.set_target(base::FilePath(FILE_PATH_LITERAL("target")));
  return base::win::CreateOrUpdateShortcutLink(
      shortcut_path, shortcut_properties, base::win::SHORTCUT_CREATE_ALWAYS);
}

base::FilePath GetShortcutPath(const base::FilePath shortcut_dir,
                               const base::FilePath::StringType shortcut_name) {
  return shortcut_dir.Append(shortcut_name).AddExtension(installer::kLnkExt);
}

}  // namespace

using WebAppShortcutWinTest = WebAppTest;

TEST_F(WebAppShortcutWinTest, GetSanitizedFileName) {
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("__")), GetSanitizedFileName(u""));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("clean filename")),
            GetSanitizedFileName(u"clean filename"));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("illegal chars")),
            GetSanitizedFileName(u"illegal*chars..."));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("path separator")),
            GetSanitizedFileName(u"path/separator"));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("_   _")),
            GetSanitizedFileName(u"***"));
}

TEST_F(WebAppShortcutWinTest, GetShortcutPaths) {
  ShortcutLocations shortcut_locations;
  EXPECT_TRUE(GetShortcutPaths(shortcut_locations).empty());

  // The location APP_MENU_LOCATION_HIDDEN is not used on Windows.
  shortcut_locations.applications_menu_location = APP_MENU_LOCATION_HIDDEN;
  EXPECT_TRUE(GetShortcutPaths(shortcut_locations).empty());

  shortcut_locations.on_desktop = true;
  shortcut_locations.applications_menu_location =
      APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
  shortcut_locations.in_quick_launch_bar = true;
  shortcut_locations.in_startup = true;
  const std::vector<base::FilePath> result =
      GetShortcutPaths(shortcut_locations);
  std::vector<ShellUtil::ShortcutLocation> expected_locations{
      ShellUtil::SHORTCUT_LOCATION_DESKTOP,
      ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR,
      ShellUtil::SHORTCUT_LOCATION_STARTUP};
  if (base::win::GetVersion() < base::win::Version::WIN10)
    expected_locations.push_back(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH);

  base::FilePath expected_result;
  for (const auto& location : expected_locations) {
    ASSERT_TRUE(ShellUtil::GetShortcutPath(location, ShellUtil::CURRENT_USER,
                                           &expected_result));
    EXPECT_NE(result.end(),
              std::find(result.begin(), result.end(), expected_result));
  }
}

TEST_F(WebAppShortcutWinTest, CheckAndSaveIcon) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath icon_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("test.ico"));

  gfx::ImageFamily image_family;
  image_family.Add(gfx::Image(CreateDefaultApplicationIcon(5)));
  EXPECT_TRUE(CheckAndSaveIcon(icon_file, image_family,
                               /*refresh_shell_icon_cache=*/false));
  // CheckAndSaveIcon() should succeed even if called multiple times.
  EXPECT_TRUE(CheckAndSaveIcon(icon_file, image_family,
                               /*refresh_shell_icon_cache=*/false));
  image_family.clear();
  image_family.Add(gfx::Image(CreateDefaultApplicationIcon(6)));
  EXPECT_TRUE(CheckAndSaveIcon(icon_file, image_family,
                               /*refresh_shell_icon_cache=*/false));
}

TEST_F(WebAppShortcutWinTest, FindAppShortcutsByProfileAndTitle) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath shortcut_dir = temp_dir.GetPath();

  const base::FilePath::StringType shortcut_name =
      FILE_PATH_LITERAL("test shortcut");
  const base::FilePath shortcut_path =
      GetShortcutPath(shortcut_dir, shortcut_name);
  const base::FilePath duplicate_shortcut_path =
      GetShortcutPath(shortcut_dir, FILE_PATH_LITERAL("test shortcut (5)"));
  const base::FilePath other_shortcut_path =
      GetShortcutPath(shortcut_dir, FILE_PATH_LITERAL("another test shortcut"));
  const base::FilePath other_profile_shortcut_path = GetShortcutPath(
      shortcut_dir, FILE_PATH_LITERAL("shortcut for another profile"));

  const base::FilePath profile_path(FILE_PATH_LITERAL("test/profile/path"));
  const base::FilePath::StringType profile_name =
      profile_path.BaseName().value();

  ASSERT_TRUE(CreateTestAppShortcut(shortcut_path, profile_name));
  ASSERT_TRUE(CreateTestAppShortcut(duplicate_shortcut_path, profile_name));
  ASSERT_TRUE(CreateTestAppShortcut(other_shortcut_path, profile_name));
  ASSERT_TRUE(CreateTestAppShortcut(other_profile_shortcut_path,
                                    FILE_PATH_LITERAL("other profile")));

  // Find |shortcut_name| by name. The specified shortcut and its duplicate
  // should be found.
  std::vector<base::FilePath> result = FindAppShortcutsByProfileAndTitle(
      shortcut_dir, profile_path, base::WideToUTF16(shortcut_name));
  EXPECT_EQ(2u, result.size());
  EXPECT_NE(result.end(),
            std::find(result.begin(), result.end(), shortcut_path));
  EXPECT_NE(result.end(),
            std::find(result.begin(), result.end(), duplicate_shortcut_path));
  EXPECT_EQ(result.end(),
            std::find(result.begin(), result.end(), other_shortcut_path));
  EXPECT_EQ(result.end(), std::find(result.begin(), result.end(),
                                    other_profile_shortcut_path));

  // Find all shortcuts for |profile_name|. The shortcuts matching that profile
  // should be found.
  result = FindAppShortcutsByProfileAndTitle(shortcut_dir, profile_path, u"");
  EXPECT_EQ(3u, result.size());
  EXPECT_NE(result.end(),
            std::find(result.begin(), result.end(), shortcut_path));
  EXPECT_NE(result.end(),
            std::find(result.begin(), result.end(), duplicate_shortcut_path));
  EXPECT_NE(result.end(),
            std::find(result.begin(), result.end(), other_shortcut_path));
  EXPECT_EQ(result.end(), std::find(result.begin(), result.end(),
                                    other_profile_shortcut_path));
}

TEST_F(WebAppShortcutWinTest,
       FindAppShortcutsByProfileAndTitleIllegalCharacters) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath shortcut_dir = temp_dir.GetPath();

  const base::FilePath::StringType shortcut_name =
      FILE_PATH_LITERAL("shortcut*with*illegal*characters...");
  const base::FilePath shortcut_path_old_syntax = GetShortcutPath(
      shortcut_dir, FILE_PATH_LITERAL("shortcut_with_illegal_characters.._"));
  const base::FilePath shortcut_path_new_syntax = GetShortcutPath(
      shortcut_dir, FILE_PATH_LITERAL("shortcut with illegal characters"));

  const base::FilePath profile_path(FILE_PATH_LITERAL("test/profile/path"));
  const base::FilePath::StringType profile_name =
      profile_path.BaseName().value();

  ASSERT_TRUE(CreateTestAppShortcut(shortcut_path_old_syntax, profile_name));
  ASSERT_TRUE(CreateTestAppShortcut(shortcut_path_new_syntax, profile_name));

  // Find shortcuts matching |shortcut_name|. Both the shortcut matching the old
  // syntax (sanitized with underscores) and new (sanitized with spaces) should
  // be found.
  std::vector<base::FilePath> result = FindAppShortcutsByProfileAndTitle(
      shortcut_dir, profile_path, base::WideToUTF16(shortcut_name));
  EXPECT_EQ(2u, result.size());
  EXPECT_NE(result.end(),
            std::find(result.begin(), result.end(), shortcut_path_old_syntax));
  EXPECT_NE(result.end(),
            std::find(result.begin(), result.end(), shortcut_path_new_syntax));
}

TEST_F(WebAppShortcutWinTest, GetIconFilePath) {
  const base::FilePath web_app_path(FILE_PATH_LITERAL("test\\web\\app\\dir"));
  EXPECT_EQ(GetIconFilePath(web_app_path, u"test app name"),
            base::FilePath(
                FILE_PATH_LITERAL("test\\web\\app\\dir\\test app name.ico")));
  EXPECT_EQ(
      GetIconFilePath(web_app_path, u"***"),
      base::FilePath(FILE_PATH_LITERAL("test\\web\\app\\dir\\_   _.ico")));
}

}  // namespace internals
}  // namespace web_app
