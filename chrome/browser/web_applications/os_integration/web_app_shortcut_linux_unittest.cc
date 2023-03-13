// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut_linux.h"

#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Provides mock environment variables values based on a stored map.
class MockEnvironment : public base::Environment {
 public:
  MockEnvironment() = default;
  MockEnvironment(const MockEnvironment&) = delete;
  MockEnvironment& operator=(const MockEnvironment&) = delete;

  void Set(base::StringPiece name, const std::string& value) {
    const std::string key(name);
    variables_[key] = value;
  }

  bool GetVar(base::StringPiece variable_name, std::string* result) override {
    const std::string key(variable_name);
    if (base::Contains(variables_, key)) {
      *result = variables_[key];
      return true;
    }

    return false;
  }

  bool SetVar(base::StringPiece variable_name,
              const std::string& new_value) override {
    ADD_FAILURE();
    return false;
  }

  bool UnSetVar(base::StringPiece variable_name) override {
    ADD_FAILURE();
    return false;
  }

 private:
  std::map<std::string, std::string> variables_;
};

class ScopedDesktopPath {
 public:
  ScopedDesktopPath() { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  ScopedDesktopPath(const ScopedDesktopPath&) = delete;
  ScopedDesktopPath& operator=(const ScopedDesktopPath&) = delete;

  base::FilePath GetPath() {
    base::FilePath desktop_path = temp_dir_.GetPath();
    EXPECT_TRUE(base::CreateDirectory(desktop_path));
    return desktop_path;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

class ScopedApplicationsPath {
 public:
  ScopedApplicationsPath() { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  ScopedApplicationsPath(const ScopedApplicationsPath&) = delete;
  ScopedApplicationsPath& operator=(const ScopedApplicationsPath&) = delete;

  base::FilePath GetPath() {
    base::FilePath applications_path = temp_dir_.GetPath();
    applications_path = applications_path.AppendASCII("applications");
    EXPECT_TRUE(base::CreateDirectory(applications_path));
    return applications_path;
  }

  base::FilePath GetDataHomePath() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

class ScopedAutoStartPath {
 public:
  ScopedAutoStartPath() { EXPECT_TRUE(temp_dir_.CreateUniqueTempDir()); }
  ScopedAutoStartPath(const ScopedAutoStartPath&) = delete;
  ScopedAutoStartPath& operator=(const ScopedAutoStartPath&) = delete;

  base::FilePath GetPath() {
    base::FilePath autostart_path = temp_dir_.GetPath();
    autostart_path = autostart_path.AppendASCII("autostart");
    EXPECT_TRUE(base::CreateDirectory(autostart_path));
    return autostart_path;
  }

  base::FilePath GetConfigHomePath() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

}  // namespace

class WebAppShortcutLinuxTest : public WebAppTest {
 public:
  base::FilePath CreateShortcutInPath(const base::FilePath& path) {
    EXPECT_TRUE(base::PathExists(path));
    base::FilePath shortcut_path = path.Append(GetTemplateFilename());
    EXPECT_TRUE(base::WriteFile(shortcut_path, ""));
    return shortcut_path;
  }

  std::unique_ptr<ShortcutInfo> GetShortcutInfo() {
    auto shortcut_info = std::make_unique<ShortcutInfo>();
    shortcut_info->app_id = GetAppId();
    shortcut_info->title = u"app";
    shortcut_info->profile_path =
        base::FilePath("/a/b/c").Append(GetProfilePath());
    gfx::ImageFamily image_family;
    SquareSizePx icon_size_in_px = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(icon_size_in_px);
    image_family.Add(gfx::Image(image_skia));
    shortcut_info->favicon = std::move(image_family);
    return shortcut_info;
  }

  std::string GetAppId() { return std::string("test_extension"); }

  std::string GetTemplateFilename() {
    return std::string("chrome-test_extension-Profile_1.desktop");
  }

  base::FilePath GetProfilePath() { return base::FilePath("Profile 1"); }

  void ValidateDeleteApplicationsLaunchXdgUtility(
      const std::vector<std::string>& argv,
      int* exit_code) {
    EXPECT_TRUE(exit_code);
    *exit_code = 0;

    std::vector<std::string> expected_argv;
    expected_argv.push_back("xdg-desktop-menu");
    expected_argv.push_back("uninstall");
    expected_argv.push_back("--mode");
    expected_argv.push_back("user");
    expected_argv.push_back("chrome-apps.directory");
    expected_argv.push_back(GetTemplateFilename());

    EXPECT_EQ(argv, expected_argv);
  }

  void ValidateCreateDesktopShortcutLaunchXdgUtility(
      const std::vector<std::string>& argv,
      int* exit_code,
      const base::FilePath& expected_applications_path,
      int invoke_count) {
    EXPECT_TRUE(exit_code);
    *exit_code = 0;

    // There are 4 calls to this function:
    //  case 0) delete the existing shortcut
    //  case 1) install the icon for the new shortcut
    //  case 2) install the new shortcut
    //  case 3) update desktop database
    EXPECT_LE(invoke_count, 4);

    std::vector<std::string> expected_argv;
    switch (invoke_count) {
      case 0:
        expected_argv.push_back("xdg-desktop-menu");
        expected_argv.push_back("uninstall");
        expected_argv.push_back("--mode");
        expected_argv.push_back("user");
        expected_argv.push_back(GetTemplateFilename());
        break;

      case 1:
        // The icon is generated to a temporary path, but the file name
        // should be known Confirm the file name is what we expect, and use
        // the parameter passed in.
        EXPECT_EQ(argv.size(), 8u);
        EXPECT_TRUE(argv[6].find("chrome-test_extension-Profile_1.png") !=
                    std::string::npos);
        expected_argv.push_back("xdg-icon-resource");
        expected_argv.push_back("install");
        expected_argv.push_back("--mode");
        expected_argv.push_back("user");
        expected_argv.push_back("--size");
        expected_argv.push_back("512");
        expected_argv.push_back(argv[6]);
        expected_argv.push_back("chrome-test_extension-Profile_1");
        break;

      case 2:
        // The desktop file is generated to a temporary path, but the file
        // name should be known Confirm the file name is what we expect, and
        // use the parameter passed in.
        EXPECT_EQ(argv.size(), 6u);
        EXPECT_TRUE(argv[4].find("chrome-apps.directory") != std::string::npos);
        EXPECT_TRUE(argv[5].find(GetTemplateFilename()) != std::string::npos);

        expected_argv.push_back("xdg-desktop-menu");
        expected_argv.push_back("install");
        expected_argv.push_back("--mode");
        expected_argv.push_back("user");
        expected_argv.push_back(argv[4]);
        expected_argv.push_back(argv[5]);
        break;

      case 3:
        expected_argv.push_back("update-desktop-database");
        expected_argv.push_back(expected_applications_path.value());
        break;
    }

    EXPECT_EQ(expected_argv, argv);
  }
};

TEST_F(WebAppShortcutLinuxTest, GetExistingShortcutLocations) {
  base::FilePath kTemplateFilepath(GetTemplateFilename());

  // No existing shortcuts.
  {
    MockEnvironment env;
    ShortcutLocations result =
        GetExistingShortcutLocations(&env, GetProfilePath(), GetAppId());
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
  }

  // Shortcut on desktop.
  {
    ScopedDesktopPath scoped_desktop_path;
    base::ScopedPathOverride user_desktop_override(
        base::DIR_USER_DESKTOP, scoped_desktop_path.GetPath());
    MockEnvironment env;

    CreateShortcutInPath(scoped_desktop_path.GetPath());
    ShortcutLocations result =
        GetExistingShortcutLocations(&env, GetProfilePath(), GetAppId());
    EXPECT_TRUE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
  }

  // Shortcut in applications directory.
  {
    ScopedApplicationsPath scoped_applications_path;
    MockEnvironment env;

    env.Set("XDG_DATA_HOME",
            scoped_applications_path.GetDataHomePath().value());
    CreateShortcutInPath(scoped_applications_path.GetPath());
    ShortcutLocations result =
        GetExistingShortcutLocations(&env, GetProfilePath(), GetAppId());
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
              result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
  }

  // Shortcut in applications directory with NoDisplay=true.
  {
    ScopedApplicationsPath scoped_applications_path;
    MockEnvironment env;

    env.Set("XDG_DATA_HOME",
            scoped_applications_path.GetDataHomePath().value());
    ASSERT_TRUE(base::WriteFile(
        scoped_applications_path.GetPath().Append(GetTemplateFilename()),
        "[Desktop Entry]\nNoDisplay=true"));
    ShortcutLocations result =
        GetExistingShortcutLocations(&env, GetProfilePath(), GetAppId());
    // Doesn't count as being in applications menu.
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_HIDDEN, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
  }

  // Shortcut in autostart directory.
  {
    ScopedAutoStartPath autostart_path;
    MockEnvironment env;

    env.Set(base::nix::kXdgConfigHomeEnvVar,
            autostart_path.GetConfigHomePath().value());
    CreateShortcutInPath(autostart_path.GetPath());

    ShortcutLocations result =
        GetExistingShortcutLocations(&env, GetProfilePath(), GetAppId());
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_TRUE(result.in_startup);
  }

  // Shortcut on desktop, in autostart folder, and in applications directory.
  {
    ScopedDesktopPath scoped_desktop_path;
    base::ScopedPathOverride user_desktop_override(
        base::DIR_USER_DESKTOP, scoped_desktop_path.GetPath());
    ScopedApplicationsPath scoped_applications_path;
    ScopedAutoStartPath autostart_path;
    MockEnvironment env;

    env.Set("XDG_DATA_HOME",
            scoped_applications_path.GetDataHomePath().value());
    env.Set(base::nix::kXdgConfigHomeEnvVar,
            autostart_path.GetConfigHomePath().value());
    CreateShortcutInPath(scoped_desktop_path.GetPath());
    CreateShortcutInPath(scoped_applications_path.GetPath());
    CreateShortcutInPath(autostart_path.GetPath());

    ShortcutLocations result =
        GetExistingShortcutLocations(&env, GetProfilePath(), GetAppId());

    EXPECT_TRUE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
              result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_TRUE(result.in_startup);
  }
}

TEST_F(WebAppShortcutLinuxTest, GetExtensionShortcutFilename) {
  EXPECT_EQ(base::FilePath("chrome-extensionid-Profile_1.desktop"),
            GetAppShortcutFilename(GetProfilePath(), "extensionid"));
}

TEST_F(WebAppShortcutLinuxTest, DeleteDesktopShortcuts) {
  ScopedDesktopPath scoped_desktop_path;
  base::FilePath desktop_shortcut_path =
      CreateShortcutInPath(scoped_desktop_path.GetPath());
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());
  ScopedAutoStartPath autostart_path;
  base::FilePath autostart_shortcut_path =
      CreateShortcutInPath(autostart_path.GetPath());
  MockEnvironment env;

  env.Set(base::nix::kXdgConfigHomeEnvVar,
          autostart_path.GetConfigHomePath().value());

  int invoke_count = 0;
  auto DeleteApplicationsLaunchXdgUtility = base::BindLambdaForTesting(
      [&](const std::vector<std::string>& argv, int* exit_code) -> bool {
        EXPECT_EQ(invoke_count, 0);
        invoke_count++;
        ValidateDeleteApplicationsLaunchXdgUtility(argv, exit_code);
        return true;
      });

  SetLaunchXdgUtilityForTesting(DeleteApplicationsLaunchXdgUtility);

  EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
  EXPECT_TRUE(base::PathExists(autostart_shortcut_path));
  EXPECT_TRUE(DeleteDesktopShortcuts(&env, GetProfilePath(), GetAppId()));
  EXPECT_EQ(invoke_count, 1);
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(base::PathExists(autostart_shortcut_path));
}

TEST_F(WebAppShortcutLinuxTest, DeleteAllDesktopShortcuts) {
  ScopedDesktopPath scoped_desktop_path;
  base::FilePath desktop_shortcut_path =
      CreateShortcutInPath(scoped_desktop_path.GetPath());
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());

  ScopedAutoStartPath autostart_path;
  base::FilePath autostart_shortcut_path =
      CreateShortcutInPath(autostart_path.GetPath());

  ScopedApplicationsPath scoped_applications_path;
  base::FilePath application_shortcut_path =
      CreateShortcutInPath(scoped_applications_path.GetPath());
  MockEnvironment env;

  env.Set("XDG_DATA_HOME", scoped_applications_path.GetDataHomePath().value());
  env.Set(base::nix::kXdgConfigHomeEnvVar,
          autostart_path.GetConfigHomePath().value());

  int invoke_count = 0;
  auto DeleteApplicationsLaunchXdgUtility = base::BindLambdaForTesting(
      [&](const std::vector<std::string>& argv, int* exit_code) -> bool {
        EXPECT_EQ(invoke_count, 0);
        invoke_count++;
        ValidateDeleteApplicationsLaunchXdgUtility(argv, exit_code);
        return true;
      });

  SetLaunchXdgUtilityForTesting(DeleteApplicationsLaunchXdgUtility);

  EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
  EXPECT_TRUE(base::PathExists(autostart_shortcut_path));
  EXPECT_TRUE(DeleteAllDesktopShortcuts(&env, GetProfilePath()));
  EXPECT_EQ(invoke_count, 1);
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(base::PathExists(autostart_shortcut_path));
}

TEST_F(WebAppShortcutLinuxTest, CreateDesktopShortcut) {
  ScopedDesktopPath scoped_desktop_path;
  ScopedApplicationsPath scoped_applications_path;
  ScopedAutoStartPath autostart_path;
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());

  MockEnvironment env;
  env.Set("XDG_DATA_HOME", scoped_applications_path.GetDataHomePath().value());
  env.Set(base::nix::kXdgConfigHomeEnvVar,
          autostart_path.GetConfigHomePath().value());

  int invoke_count = 0;
  LaunchXdgUtilityForTesting CreateDesktopShortcutLaunchXdgUtility =
      base::BindLambdaForTesting([&](const std::vector<std::string>& argv,
                                     int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(
            argv, exit_code, scoped_applications_path.GetPath(), invoke_count);
        invoke_count++;

        if (invoke_count < 4)
          SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  EXPECT_TRUE(CreateDesktopShortcut(&env, *shortcut_info, locations));
  EXPECT_EQ(invoke_count, 4);

  // At this point, we've already validated creation in the Application menu
  // because of the hook into XdgUtilityForTesting.
  // Validate the shortcut was created, and the contents are what we expect
  // them to be.

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
            "", false, "", shortcut_info->actions);

    base::FilePath desktop_shortcut_path =
        scoped_desktop_path.GetPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(desktop_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(desktop_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
            "", false, kRunOnOsLoginModeWindowed, shortcut_info->actions);
    base::FilePath autostart_shortcut_path =
        autostart_path.GetPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

// Validates protocols are only added to the applications folder
// .desktop file.
TEST_F(WebAppShortcutLinuxTest, CreateDesktopShortcutWithProtocols) {
  ScopedDesktopPath scoped_desktop_path;
  ScopedApplicationsPath scoped_applications_path;
  ScopedAutoStartPath autostart_path;
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());

  MockEnvironment env;
  env.Set("XDG_DATA_HOME", scoped_applications_path.GetDataHomePath().value());
  env.Set(base::nix::kXdgConfigHomeEnvVar,
          autostart_path.GetConfigHomePath().value());

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  // Add protocol handlers
  shortcut_info->protocol_handlers.emplace("mailto");
  shortcut_info->protocol_handlers.emplace("web+testing");

  int invoke_count = 0;
  LaunchXdgUtilityForTesting CreateDesktopShortcutLaunchXdgUtility =
      base::BindLambdaForTesting([&](const std::vector<std::string>& argv,
                                     int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(
            argv, exit_code, scoped_applications_path.GetPath(), invoke_count);

        // Validate protocols were added to contents correctly
        if (invoke_count == 2) {
          std::string expected_contents =
              shell_integration_linux::GetDesktopFileContents(
                  shell_integration_linux::internal::GetChromeExePath(),
                  GenerateApplicationNameFromInfo(*shortcut_info),
                  shortcut_info->url, shortcut_info->app_id,
                  shortcut_info->title, "chrome-test_extension-Profile_1",
                  shortcut_info->profile_path, "",
                  "x-scheme-handler/mailto;x-scheme-handler/web+testing", false,
                  "", shortcut_info->actions);

          base::FilePath application_shortcut_path(argv[5]);
          EXPECT_TRUE(base::PathExists(application_shortcut_path));

          std::string actual_contents;
          EXPECT_TRUE(base::ReadFileToString(application_shortcut_path,
                                             &actual_contents));
          EXPECT_EQ(expected_contents, actual_contents);
        }
        invoke_count++;

        if (invoke_count < 4)
          SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  EXPECT_TRUE(CreateDesktopShortcut(&env, *shortcut_info, locations));
  EXPECT_EQ(invoke_count, 4);

  // At this point, we've already validated creation in the Application menu
  // because of the hook into XdgUtilityForTesting.
  // Validate the shortcut was created, and the contents are what we expect
  // them to be.

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
            "", false, "", shortcut_info->actions);

    base::FilePath desktop_shortcut_path =
        scoped_desktop_path.GetPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(desktop_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(desktop_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
            "", false, kRunOnOsLoginModeWindowed, shortcut_info->actions);
    base::FilePath autostart_shortcut_path =
        autostart_path.GetPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

TEST_F(WebAppShortcutLinuxTest,
       CreateDesktopShortcutWithNonExistantUserDesktopDir) {
  ScopedDesktopPath scoped_desktop_path;
  ScopedApplicationsPath scoped_applications_path;
  ScopedAutoStartPath autostart_path;
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());

  MockEnvironment env;
  env.Set("XDG_DATA_HOME", scoped_applications_path.GetDataHomePath().value());
  env.Set(base::nix::kXdgConfigHomeEnvVar,
          autostart_path.GetConfigHomePath().value());

  auto CreateDesktopShortcutLaunchXdgUtility = base::BindLambdaForTesting(
      [&](const std::vector<std::string>& argv, int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(
            argv, exit_code, scoped_applications_path.GetPath(), 1);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  locations.applications_menu_location = APP_MENU_LOCATION_NONE;

  // Force a failure by deleting the |scoped_desktop_path| location.
  base::FilePath desktop_path = scoped_desktop_path.GetPath();
  EXPECT_TRUE(DeletePathRecursively(desktop_path));
  EXPECT_FALSE(CreateDesktopShortcut(&env, *shortcut_info, locations));

  // At this point, we've already validated creation in the Application menu
  // because of the hook into XdgUtilityForTesting.
  // Validate the shortcut was created, and the contents are what we expect
  // them to be.
  std::string expected_contents =
      shell_integration_linux::GetDesktopFileContents(
          shell_integration_linux::internal::GetChromeExePath(),
          GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
          shortcut_info->app_id, shortcut_info->title,
          "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
          "", false, kRunOnOsLoginModeWindowed, shortcut_info->actions);

  // |scoped_desktop_path| was deleted earlier, confirm it wasn't recreated.
  EXPECT_FALSE(base::DirectoryExists(desktop_path));

  // However, despite the failure, the contents of the autostart file
  // should still be correct.
  {
    base::FilePath autostart_shortcut_path =
        autostart_path.GetPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

TEST_F(WebAppShortcutLinuxTest, CreateDesktopShortcutEmptyExtension) {
  ScopedDesktopPath scoped_desktop_path;
  ScopedApplicationsPath scoped_applications_path;
  ScopedAutoStartPath autostart_path;
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());
  MockEnvironment env;
  env.Set("XDG_DATA_HOME", scoped_applications_path.GetDataHomePath().value());
  env.Set(base::nix::kXdgConfigHomeEnvVar,
          autostart_path.GetConfigHomePath().value());

  int invoke_count = 0;
  LaunchXdgUtilityForTesting CreateDesktopShortcutLaunchXdgUtility =
      base::BindLambdaForTesting([&](const std::vector<std::string>& argv,
                                     int* exit_code) -> bool {
        EXPECT_TRUE(exit_code);
        *exit_code = 0;

        // There are 3 calls to this function:
        //  case 0) install the icon for the new shortcut
        //  case 1) install the new shortcut
        //  case 2) update desktop database
        EXPECT_LE(invoke_count, 3);

        std::vector<std::string> expected_argv;
        switch (invoke_count) {
          case 0:
            // The icon is generated to a temporary path, but the file name
            // should be known Confirm the file name is what we expect, and use
            // the parameter passed in.
            EXPECT_EQ(argv.size(), 8u);
            EXPECT_TRUE(argv[6].find("chrome-https___example.com_.png") !=
                        std::string::npos);
            expected_argv.push_back("xdg-icon-resource");
            expected_argv.push_back("install");
            expected_argv.push_back("--mode");
            expected_argv.push_back("user");
            expected_argv.push_back("--size");
            expected_argv.push_back("512");
            expected_argv.push_back(argv[6]);
            expected_argv.push_back("chrome-https___example.com_");
            break;

          case 1:
            // The desktop file is generated to a temporary path, but the file
            // name should be known Confirm the file name is what we expect, and
            // use the parameter passed in.
            EXPECT_EQ(argv.size(), 6u);
            EXPECT_TRUE(argv[4].find("chrome-apps.directory") !=
                        std::string::npos);
            EXPECT_TRUE(argv[5].find("chrome-https___example.com_.desktop") !=
                        std::string::npos);

            expected_argv.push_back("xdg-desktop-menu");
            expected_argv.push_back("install");
            expected_argv.push_back("--mode");
            expected_argv.push_back("user");
            expected_argv.push_back(argv[4]);
            expected_argv.push_back(argv[5]);
            break;

          case 2:
            expected_argv.push_back("update-desktop-database");
            expected_argv.push_back(scoped_applications_path.GetPath().value());
            break;
        }

        invoke_count++;
        EXPECT_EQ(expected_argv, argv);
        if (invoke_count < 3)
          SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  shortcut_info->app_id = "";
  shortcut_info->url = GURL("https://example.com");

  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  EXPECT_TRUE(CreateDesktopShortcut(&env, *shortcut_info, locations));
  EXPECT_EQ(invoke_count, 3);

  // At this point, we've already validated creation in the Application menu
  // because of the hook into XdgUtilityForTesting.
  // Validate the shortcut was created, and the contents are what we expect
  // them to be.

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-https___example.com_", shortcut_info->profile_path, "", "",
            false, "", shortcut_info->actions);

    base::FilePath desktop_shortcut_path = scoped_desktop_path.GetPath().Append(
        "chrome-https___example.com_.desktop");
    ASSERT_TRUE(base::PathExists(desktop_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(desktop_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-https___example.com_", shortcut_info->profile_path, "", "",
            false, kRunOnOsLoginModeWindowed, shortcut_info->actions);

    base::FilePath autostart_shortcut_path =
        autostart_path.GetPath().Append("chrome-https___example.com_.desktop");
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

TEST_F(WebAppShortcutLinuxTest, UpdateDesktopShortcuts) {
  ScopedDesktopPath scoped_desktop_path;
  ScopedApplicationsPath scoped_applications_path;
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());
  MockEnvironment env;
  env.Set("XDG_DATA_HOME", scoped_applications_path.GetDataHomePath().value());
  CreateShortcutInPath(scoped_desktop_path.GetPath());
  CreateShortcutInPath(scoped_applications_path.GetPath());

  int invoke_count = 0;
  LaunchXdgUtilityForTesting CreateDesktopShortcutLaunchXdgUtility =
      base::BindLambdaForTesting([&](const std::vector<std::string>& argv,
                                     int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(
            argv, exit_code, scoped_applications_path.GetPath(), invoke_count);
        invoke_count++;

        if (invoke_count < 4)
          SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();

  UpdateDesktopShortcuts(&env, *shortcut_info);
  EXPECT_EQ(invoke_count, 4);

  // At this point, we've already validated creation in the Application menu
  // because of the hook into XdgUtilityForTesting.
  // Validate the shortcut was created, and the contents are what we expect
  // them to be.
  std::string expected_contents =
      shell_integration_linux::GetDesktopFileContents(
          shell_integration_linux::internal::GetChromeExePath(),
          GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
          shortcut_info->app_id, shortcut_info->title,
          "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
          "", false, "", shortcut_info->actions);

  base::FilePath desktop_shortcut_path =
      scoped_desktop_path.GetPath().Append(GetTemplateFilename());
  ASSERT_TRUE(base::PathExists(desktop_shortcut_path));

  std::string actual_contents;
  ASSERT_TRUE(base::ReadFileToString(desktop_shortcut_path, &actual_contents));
  EXPECT_EQ(expected_contents, actual_contents);
}

TEST_F(WebAppShortcutLinuxTest, GetShortcutLocations) {
  // No existing shortcuts.
  {
    MockEnvironment env;
    ShortcutLocations locations;
    std::vector<base::FilePath> result =
        GetShortcutLocations(&env, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 0u);
  }

  // Shortcut on desktop.
  {
    ScopedDesktopPath scoped_desktop_path;
    base::ScopedPathOverride user_desktop_override(
        base::DIR_USER_DESKTOP, scoped_desktop_path.GetPath());
    base::FilePath desktop_shortcut_path =
        CreateShortcutInPath(scoped_desktop_path.GetPath());

    MockEnvironment env;
    ShortcutLocations locations;
    locations.on_desktop = true;

    EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
    std::vector<base::FilePath> result =
        GetShortcutLocations(&env, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], desktop_shortcut_path);
  }

  // Shortcut in autostart directory.
  {
    ScopedAutoStartPath autostart_path;
    base::FilePath autostart_shortcut_path =
        CreateShortcutInPath(autostart_path.GetPath());
    MockEnvironment env;

    env.Set(base::nix::kXdgConfigHomeEnvVar,
            autostart_path.GetConfigHomePath().value());

    ShortcutLocations locations;
    locations.in_startup = true;

    EXPECT_TRUE(base::PathExists(autostart_shortcut_path));
    std::vector<base::FilePath> result =
        GetShortcutLocations(&env, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], autostart_shortcut_path);
  }

  // Shortcut on desktop, and in autostart folder.
  {
    ScopedDesktopPath scoped_desktop_path;
    base::ScopedPathOverride user_desktop_override(
        base::DIR_USER_DESKTOP, scoped_desktop_path.GetPath());
    base::FilePath desktop_shortcut_path =
        CreateShortcutInPath(scoped_desktop_path.GetPath());
    ScopedAutoStartPath autostart_path;
    base::FilePath autostart_shortcut_path =
        CreateShortcutInPath(autostart_path.GetPath());
    MockEnvironment env;

    env.Set(base::nix::kXdgConfigHomeEnvVar,
            autostart_path.GetConfigHomePath().value());

    ShortcutLocations locations;
    locations.in_startup = true;
    locations.on_desktop = true;

    EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
    EXPECT_TRUE(base::PathExists(autostart_shortcut_path));
    std::vector<base::FilePath> result =
        GetShortcutLocations(&env, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], desktop_shortcut_path);
    EXPECT_EQ(result[1], autostart_shortcut_path);
  }
}

// Validates Shortcut Menu actions are only added to the applications folder
// .desktop file.
TEST_F(WebAppShortcutLinuxTest, CreateDesktopShortcutWithShortcutMenuActions) {
  ScopedDesktopPath scoped_desktop_path;
  ScopedApplicationsPath scoped_applications_path;
  ScopedAutoStartPath autostart_path;
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 scoped_desktop_path.GetPath());

  MockEnvironment env;
  env.Set("XDG_DATA_HOME", scoped_applications_path.GetDataHomePath().value());
  env.Set(base::nix::kXdgConfigHomeEnvVar,
          autostart_path.GetConfigHomePath().value());

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  // Add actions
  shortcut_info->actions.emplace("action1", "Action 1",
                                 GURL("https://example.com/action1"));
  shortcut_info->actions.emplace("action2", "Action 2",
                                 GURL("https://example.com/action2"));
  shortcut_info->actions.emplace("action3", "Action 3",
                                 GURL("https://example.com/action3"));
  shortcut_info->actions.emplace("action4", "Action 4",
                                 GURL("https://example.com/action4"));

  int invoke_count = 0;
  LaunchXdgUtilityForTesting CreateDesktopShortcutLaunchXdgUtility =
      base::BindLambdaForTesting([&](const std::vector<std::string>& argv,
                                     int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(
            argv, exit_code, scoped_applications_path.GetPath(), invoke_count);

        // Validate actions were added to contents correctly
        if (invoke_count == 2) {
          std::string expected_contents =
              shell_integration_linux::GetDesktopFileContents(
                  shell_integration_linux::internal::GetChromeExePath(),
                  GenerateApplicationNameFromInfo(*shortcut_info),
                  shortcut_info->url, shortcut_info->app_id,
                  shortcut_info->title, "chrome-test_extension-Profile_1",
                  shortcut_info->profile_path, "", "", false, "",
                  shortcut_info->actions);

          base::FilePath application_shortcut_path(argv[5]);
          EXPECT_TRUE(base::PathExists(application_shortcut_path));

          std::string actual_contents;
          EXPECT_TRUE(base::ReadFileToString(application_shortcut_path,
                                             &actual_contents));
          EXPECT_EQ(expected_contents, actual_contents);
        }
        invoke_count++;

        if (invoke_count < 4)
          SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  EXPECT_TRUE(CreateDesktopShortcut(&env, *shortcut_info, locations));
  EXPECT_EQ(invoke_count, 4);

  // At this point, we've already validated creation in the Application menu
  // because of the hook into XdgUtilityForTesting.
  // Validate the shortcut was created, and the contents are what we expect
  // them to be.

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
            "", false, "", shortcut_info->actions);

    base::FilePath desktop_shortcut_path =
        scoped_desktop_path.GetPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(desktop_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(desktop_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }

  {
    std::string expected_contents =
        shell_integration_linux::GetDesktopFileContents(
            shell_integration_linux::internal::GetChromeExePath(),
            GenerateApplicationNameFromInfo(*shortcut_info), shortcut_info->url,
            shortcut_info->app_id, shortcut_info->title,
            "chrome-test_extension-Profile_1", shortcut_info->profile_path, "",
            "", false, kRunOnOsLoginModeWindowed, shortcut_info->actions);
    base::FilePath autostart_shortcut_path =
        autostart_path.GetPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

}  // namespace web_app
