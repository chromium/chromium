// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"

#include <utility>
#include <vector>

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/win/shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_skia.h"

namespace web_app::internals {
namespace {

constexpr char kWebAppId[] = "123";

bool CreateTestAppShortcut(const base::FilePath& shortcut_path,
                           const base::FilePath::StringType& profile_name) {
  base::CommandLine args_cl(base::CommandLine::NO_PROGRAM);
  args_cl.AppendSwitchNative(switches::kProfileDirectory, profile_name);
  args_cl.AppendSwitchASCII(switches::kAppId, kWebAppId);

  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_arguments(args_cl.GetArgumentsString());
  shortcut_properties.set_target(base::FilePath(FILE_PATH_LITERAL("target")));
  return base::win::CreateOrUpdateShortcutLink(
      shortcut_path, shortcut_properties,
      base::win::ShortcutOperation::kCreateAlways);
}

base::FilePath GetShortcutPath(
    const base::FilePath& shortcut_dir,
    const base::FilePath::StringType& shortcut_name) {
  return shortcut_dir.Append(shortcut_name).AddExtension(installer::kLnkExt);
}

void CreateAndVerifyTestAppShortcut(
    const base::FilePath::StringType& shortcut_name,
    const base::FilePath& shortcut_dir,
    const base::FilePath& profile_path) {
  const base::FilePath shortcut_path =
      GetShortcutPath(shortcut_dir, shortcut_name);
  EXPECT_TRUE(
      CreateTestAppShortcut(shortcut_path, profile_path.BaseName().value()));
  const std::vector<base::FilePath> result = FindAppShortcutsByProfileAndTitle(
      shortcut_dir, profile_path, base::WideToUTF16(shortcut_name));
  EXPECT_EQ(1u, result.size());
}

// Synchronous wrapper for UpdatePlatformShortcuts for ease of testing.
Result UpdatePlatformShortcuts(
    const base::FilePath& web_app_path,
    const std::u16string& old_app_title,
    std::optional<ShortcutLocations> user_specified_locations,
    const ShortcutInfo& shortcut_info) {
  base::test::TestFuture<Result> result;
  web_app::internals::UpdatePlatformShortcuts(
      web_app_path, old_app_title, std::move(user_specified_locations),
      result.GetCallback(), shortcut_info);
  return result.Get();
}

}  // namespace

class WebAppShortcutWinTest : public WebAppTest {
 protected:
  base::ScopedPathOverride override_taskbar_pin{base::DIR_TASKBAR_PINS};
  base::ScopedPathOverride override_implicit_apps{
      base::DIR_IMPLICIT_APP_SHORTCUTS};
};

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

  base::FilePath expected_result;
  for (const auto& location : expected_locations) {
    ASSERT_TRUE(ShellUtil::GetShortcutPath(location, ShellUtil::CURRENT_USER,
                                           &expected_result));
    EXPECT_TRUE(base::Contains(result, expected_result));
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
  EXPECT_TRUE(base::Contains(result, shortcut_path));
  EXPECT_TRUE(base::Contains(result, duplicate_shortcut_path));
  EXPECT_FALSE(base::Contains(result, other_shortcut_path));
  EXPECT_FALSE(base::Contains(result, other_profile_shortcut_path));

  // Find all shortcuts for |profile_name|. The shortcuts matching that profile
  // should be found.
  result = FindAppShortcutsByProfileAndTitle(shortcut_dir, profile_path, u"");
  EXPECT_EQ(3u, result.size());
  EXPECT_TRUE(base::Contains(result, shortcut_path));
  EXPECT_TRUE(base::Contains(result, duplicate_shortcut_path));
  EXPECT_TRUE(base::Contains(result, other_shortcut_path));
  EXPECT_FALSE(base::Contains(result, other_profile_shortcut_path));
}

TEST_F(WebAppShortcutWinTest,
       FindAppShortcutsByProfileAndTitleIllegalCharacters) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath shortcut_dir = temp_dir.GetPath();

  const base::FilePath::StringType shortcut_name =
      FILE_PATH_LITERAL("shortcut*with*illegal*characters...");
  const base::FilePath sanitized_shortcut_path = GetShortcutPath(
      shortcut_dir, FILE_PATH_LITERAL("shortcut with illegal characters"));

  const base::FilePath profile_path(FILE_PATH_LITERAL("test/profile/path"));
  const base::FilePath::StringType profile_name =
      profile_path.BaseName().value();

  ASSERT_TRUE(CreateTestAppShortcut(sanitized_shortcut_path, profile_name));

  // Find shortcuts matching `shortcut_name`. A shortcut with the sanitized name
  // should be found.
  std::vector<base::FilePath> result = FindAppShortcutsByProfileAndTitle(
      shortcut_dir, profile_path, base::WideToUTF16(shortcut_name));
  EXPECT_EQ(1u, result.size());
  EXPECT_TRUE(base::Contains(result, sanitized_shortcut_path));
}

TEST_F(WebAppShortcutWinTest, UpdatePlatformShortcuts) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath shortcut_dir = temp_dir.GetPath();

  const base::FilePath::StringType shortcut_name =
      FILE_PATH_LITERAL("test shortcut");
  base::FilePath shortcut_path = GetShortcutPath(shortcut_dir, shortcut_name);

  const base::FilePath profile_path(FILE_PATH_LITERAL("test/profile/web_app"));
  const base::FilePath::StringType profile_name =
      profile_path.BaseName().value();

  ASSERT_NO_FATAL_FAILURE(CreateAndVerifyTestAppShortcut(
      shortcut_name, shortcut_dir, profile_path));

  // Create shortcut in overridden taskbar dir.
  base::FilePath taskbar_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TASKBAR_PINS, &taskbar_dir));
  ASSERT_NO_FATAL_FAILURE(
      CreateAndVerifyTestAppShortcut(shortcut_name, taskbar_dir, profile_path));

  // Create shortcut in overridden implicit apps dir.
  base::FilePath implicit_apps_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_IMPLICIT_APP_SHORTCUTS,
                                     &implicit_apps_dir));
  base::FilePath implicit_apps_sub_dir = implicit_apps_dir.AppendASCII("abcde");
  ASSERT_TRUE(base::CreateDirectory(implicit_apps_sub_dir));
  ASSERT_NO_FATAL_FAILURE(CreateAndVerifyTestAppShortcut(
      shortcut_name, implicit_apps_sub_dir, profile_path));

  // Create an icon file for the web app.
  const base::FilePath icon_file =
      GetIconFilePath(shortcut_dir, u"test shortcut");
  gfx::ImageFamily image_family;
  image_family.Add(gfx::Image(CreateDefaultApplicationIcon(5)));
  EXPECT_TRUE(CheckAndSaveIcon(icon_file, image_family,
                               /*refresh_shell_icon_cache=*/false));

  // Update the web app with a new title.
  ShortcutInfo new_shortcut_info;
  new_shortcut_info.title = u"new title";
  new_shortcut_info.profile_path = profile_path;
  new_shortcut_info.profile_name = base::WideToUTF8(profile_name);
  new_shortcut_info.app_id = kWebAppId;

  // Set the favicon to be the same as the original icon.
  new_shortcut_info.favicon = std::move(image_family);

  UpdatePlatformShortcuts(shortcut_dir, base::WideToUTF16(shortcut_name),
                          /*user_specified_locations=*/std::nullopt,
                          new_shortcut_info);
  // The shortcut with the old title should be deleted from the shortcut
  // dir, the taskbar dir, and the implicit apps subdir.
  std::vector<base::FilePath> result = FindAppShortcutsByProfileAndTitle(
      shortcut_dir, profile_path, base::WideToUTF16(shortcut_name));
  EXPECT_EQ(0u, result.size());
  result = FindAppShortcutsByProfileAndTitle(taskbar_dir, profile_path,
                                             base::WideToUTF16(shortcut_name));
  EXPECT_EQ(0u, result.size());
  result = FindAppShortcutsByProfileAndTitle(
      implicit_apps_sub_dir, profile_path, base::WideToUTF16(shortcut_name));
  EXPECT_EQ(0u, result.size());
  // The shortcut with the new title should be found in the shortcut dir, the
  // taskbar dir, and the implicit_apps subdir.
  result = FindAppShortcutsByProfileAndTitle(shortcut_dir, profile_path,
                                             new_shortcut_info.title);
  EXPECT_EQ(1u, result.size());
  result = FindAppShortcutsByProfileAndTitle(taskbar_dir, profile_path,
                                             new_shortcut_info.title);
  EXPECT_EQ(1u, result.size());
  result = FindAppShortcutsByProfileAndTitle(
      implicit_apps_sub_dir, profile_path, new_shortcut_info.title);
  EXPECT_EQ(1u, result.size());
}

TEST_F(WebAppShortcutWinTest, UpdatePlatformShortcutsAppIdentityChange) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath shortcut_dir = temp_dir.GetPath();

  const base::FilePath::StringType shortcut_name =
      FILE_PATH_LITERAL("test shortcut");

  const base::FilePath profile_path(FILE_PATH_LITERAL("test/profile/web_app"));
  const base::FilePath::StringType profile_name =
      profile_path.BaseName().value();

  ASSERT_NO_FATAL_FAILURE(CreateAndVerifyTestAppShortcut(
      shortcut_name, shortcut_dir, profile_path));

  // Create a 16x16 icon file for the web app.
  const base::FilePath icon_file =
      GetIconFilePath(shortcut_dir, u"test shortcut");
  gfx::ImageFamily image_family;
  image_family.Add(gfx::Image(CreateDefaultApplicationIcon(16)));
  EXPECT_TRUE(CheckAndSaveIcon(icon_file, image_family,
                               /*refresh_shell_icon_cache=*/false));

  // Update the web app with a new title and a new icon family.
  ShortcutInfo new_shortcut_info;
  new_shortcut_info.title = u"new title";
  new_shortcut_info.profile_path = profile_path;
  new_shortcut_info.profile_name = base::WideToUTF8(profile_name);
  new_shortcut_info.app_id = kWebAppId;
  gfx::ImageFamily new_image_family;
  new_image_family.Add(gfx::Image(CreateDefaultApplicationIcon(32)));
  new_shortcut_info.favicon = std::move(new_image_family);

  UpdatePlatformShortcuts(shortcut_dir, base::WideToUTF16(shortcut_name),
                          /*user_specified_locations=*/std::nullopt,
                          new_shortcut_info);

  // The shortcut with the old title should have been deleted.
  std::vector<base::FilePath> result = FindAppShortcutsByProfileAndTitle(
      shortcut_dir, profile_path, base::WideToUTF16(shortcut_name));
  EXPECT_EQ(0u, result.size());

  // When an app changes both title and icons, the icon file and shortcuts
  // should reflect that. For example, the old icon file and its checksum should
  // have been deleted:
  EXPECT_FALSE(base::PathExists(icon_file));
  EXPECT_FALSE(base::PathExists(base::FilePath(
      icon_file.ReplaceExtension(FILE_PATH_LITERAL(".ico.md5")))));

  // The shortcut with the new title should now be in the shortcut dir.
  result = FindAppShortcutsByProfileAndTitle(shortcut_dir, profile_path,
                                             new_shortcut_info.title);
  EXPECT_EQ(1u, result.size());

  // A new icon file (and checksum) should have been created.
  const base::FilePath new_icon_file =
      GetIconFilePath(shortcut_dir, u"new title");
  EXPECT_TRUE(base::PathExists(base::FilePath(
      new_icon_file.ReplaceExtension(FILE_PATH_LITERAL(".ico.md5")))));

  // The shortcut should have been updated to use the new icon.
  using base::win::ShortcutProperties;
  ShortcutProperties shortcut_properties;
  base::FilePath shortcut_file =
      shortcut_dir.Append(FILE_PATH_LITERAL("new title.lnk"));
  EXPECT_TRUE(base::PathExists(shortcut_file));
  EXPECT_TRUE(base::win::ResolveShortcutProperties(
      shortcut_file, ShortcutProperties::PROPERTIES_ICON,
      &shortcut_properties));
  EXPECT_EQ(new_icon_file, shortcut_properties.icon);
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

}  // namespace web_app::internals
