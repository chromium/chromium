// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut_linux.h"

#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string_view>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace web_app {

class WebAppShortcutLinuxTest : public WebAppTest {
 public:
  base::FilePath CreateShortcutInPath(const base::FilePath& path) {
    EXPECT_TRUE(base::PathExists(path));
    base::FilePath shortcut_path = path.Append(GetTemplateFilename());
    EXPECT_TRUE(base::WriteFile(shortcut_path, ""));
    return shortcut_path;
  }

  void DeleteShortcutInPath(const base::FilePath& path) {
    EXPECT_TRUE(base::PathExists(path));
    base::FilePath shortcut_path = path.Append(GetTemplateFilename());
    EXPECT_TRUE(base::DeleteFile(shortcut_path));
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
        ASSERT_GT(argv.size(), 6u)
            << base::JoinString(base::make_span(argv), " ");
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

    EXPECT_THAT(argv, testing::ElementsAreArray(expected_argv));
  }

  base::FilePath GetDesktopPath() {
    return OsIntegrationTestOverrideImpl::Get()->desktop();
  }
  base::FilePath GetApplicationsPath() {
    return OsIntegrationTestOverrideImpl::Get()->applications();
  }
  base::FilePath GetAutostartPath() {
    return OsIntegrationTestOverrideImpl::Get()->startup();
  }

 private:
  OsIntegrationTestOverrideBlockingRegistration os_integration_override_;
};

TEST_F(WebAppShortcutLinuxTest, GetExistingShortcutLocations) {
  base::FilePath kTemplateFilepath(GetTemplateFilename());

  // No existing shortcuts.
  {
    ShortcutLocations result = GetExistingShortcutLocations(
        /*env=*/nullptr, GetProfilePath(), GetAppId());
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
  }

  // Shortcut on desktop.
  {
    CreateShortcutInPath(GetDesktopPath());
    ShortcutLocations result = GetExistingShortcutLocations(
        /*env=*/nullptr, GetProfilePath(), GetAppId());
    EXPECT_TRUE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
    // Delete to reset the state.
    DeleteShortcutInPath(GetDesktopPath());
  }

  // Shortcut in applications directory.
  {
    CreateShortcutInPath(GetApplicationsPath());
    ShortcutLocations result = GetExistingShortcutLocations(
        /*env=*/nullptr, GetProfilePath(), GetAppId());
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
              result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
    // Delete to reset the state.
    DeleteShortcutInPath(GetApplicationsPath());
  }

  // Shortcut in applications directory with NoDisplay=true.
  {
    ASSERT_TRUE(
        base::WriteFile(GetApplicationsPath().Append(GetTemplateFilename()),
                        "[Desktop Entry]\nNoDisplay=true"));
    ShortcutLocations result = GetExistingShortcutLocations(
        /*env=*/nullptr, GetProfilePath(), GetAppId());
    // Doesn't count as being in applications menu.
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_HIDDEN, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.in_startup);
    // Delete to reset the state.
    DeleteShortcutInPath(GetApplicationsPath());
  }

  // Shortcut in autostart directory.
  {
    CreateShortcutInPath(GetAutostartPath());
    ShortcutLocations result = GetExistingShortcutLocations(
        /*env=*/nullptr, GetProfilePath(), GetAppId());
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_TRUE(result.in_startup);
    // Delete to reset the state.
    DeleteShortcutInPath(GetAutostartPath());
  }

  // Shortcut on desktop, in autostart folder, and in applications directory.
  {
    CreateShortcutInPath(GetDesktopPath());
    CreateShortcutInPath(GetApplicationsPath());
    CreateShortcutInPath(GetAutostartPath());

    ShortcutLocations result = GetExistingShortcutLocations(
        /*env=*/nullptr, GetProfilePath(), GetAppId());

    EXPECT_TRUE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
              result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_TRUE(result.in_startup);
  }
}

TEST_F(WebAppShortcutLinuxTest, GetExtensionShortcutFilename) {
  EXPECT_EQ(base::FilePath("chrome-extensionid-Profile_1.desktop"),
            GetAppDesktopShortcutFilename(GetProfilePath(), "extensionid"));
}

TEST_F(WebAppShortcutLinuxTest, DeleteDesktopShortcuts) {
  base::FilePath autostart_shortcut_path =
      CreateShortcutInPath(GetAutostartPath());
  base::FilePath desktop_shortcut_path = CreateShortcutInPath(GetDesktopPath());
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
  EXPECT_TRUE(
      DeleteDesktopShortcuts(/*env=*/nullptr, GetProfilePath(), GetAppId()));
  EXPECT_EQ(invoke_count, 1);
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(base::PathExists(autostart_shortcut_path));
}

TEST_F(WebAppShortcutLinuxTest, DeleteAllDesktopShortcuts) {
  base::FilePath desktop_shortcut_path = CreateShortcutInPath(GetDesktopPath());
  base::ScopedPathOverride user_desktop_override(base::DIR_USER_DESKTOP,
                                                 GetDesktopPath());

  base::FilePath autostart_shortcut_path =
      CreateShortcutInPath(GetAutostartPath());

  base::FilePath application_shortcut_path =
      CreateShortcutInPath(GetApplicationsPath());
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
  EXPECT_TRUE(DeleteAllDesktopShortcuts(/*env=*/nullptr, GetProfilePath()));
  EXPECT_EQ(invoke_count, 1);
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(base::PathExists(autostart_shortcut_path));
}

TEST_F(WebAppShortcutLinuxTest, CreateDesktopShortcut) {

  int invoke_count = 0;
  LaunchXdgUtilityForTesting CreateDesktopShortcutLaunchXdgUtility =
      base::BindLambdaForTesting([&](const std::vector<std::string>& argv,
                                     int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(
            argv, exit_code, GetApplicationsPath(), invoke_count);
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

  EXPECT_TRUE(
      CreateDesktopShortcut(/*env=*/nullptr, *shortcut_info, locations));
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
        GetDesktopPath().Append(GetTemplateFilename());
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
        GetAutostartPath().Append(GetTemplateFilename());
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
            argv, exit_code, GetApplicationsPath(), invoke_count);

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

  EXPECT_TRUE(
      CreateDesktopShortcut(/*env=*/nullptr, *shortcut_info, locations));
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
        GetDesktopPath().Append(GetTemplateFilename());
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
        GetAutostartPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

TEST_F(WebAppShortcutLinuxTest,
       CreateDesktopShortcutWithNonExistantUserDesktopDir) {
  auto CreateDesktopShortcutLaunchXdgUtility = base::BindLambdaForTesting(
      [&](const std::vector<std::string>& argv, int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(argv, exit_code,
                                                      GetApplicationsPath(), 1);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();
  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  locations.applications_menu_location = APP_MENU_LOCATION_NONE;

  // Force a failure by deleting the |scoped_desktop_path| location.
  base::FilePath desktop_path = GetDesktopPath();
  EXPECT_TRUE(DeletePathRecursively(desktop_path));
  EXPECT_FALSE(
      CreateDesktopShortcut(/*env=*/nullptr, *shortcut_info, locations));

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
        GetAutostartPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

TEST_F(WebAppShortcutLinuxTest, CreateDesktopShortcutEmptyExtension) {
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
            expected_argv.push_back(GetApplicationsPath().value());
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

  EXPECT_TRUE(
      CreateDesktopShortcut(/*env=*/nullptr, *shortcut_info, locations));
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

    base::FilePath desktop_shortcut_path =
        GetDesktopPath().Append("chrome-https___example.com_.desktop");
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
        GetAutostartPath().Append("chrome-https___example.com_.desktop");
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

TEST_F(WebAppShortcutLinuxTest, UpdateDesktopShortcuts) {
  CreateShortcutInPath(GetDesktopPath());
  CreateShortcutInPath(GetApplicationsPath());

  int invoke_count = 0;
  LaunchXdgUtilityForTesting CreateDesktopShortcutLaunchXdgUtility =
      base::BindLambdaForTesting([&](const std::vector<std::string>& argv,
                                     int* exit_code) -> bool {
        ValidateCreateDesktopShortcutLaunchXdgUtility(
            argv, exit_code, GetApplicationsPath(), invoke_count);
        invoke_count++;

        if (invoke_count < 4)
          SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);
        return true;
      });

  SetLaunchXdgUtilityForTesting(CreateDesktopShortcutLaunchXdgUtility);

  std::unique_ptr<ShortcutInfo> shortcut_info = GetShortcutInfo();

  UpdateDesktopShortcuts(/*env=*/nullptr, *shortcut_info,
                         /*user_specified_locations=*/std::nullopt);
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
      GetDesktopPath().Append(GetTemplateFilename());
  ASSERT_TRUE(base::PathExists(desktop_shortcut_path));

  std::string actual_contents;
  ASSERT_TRUE(base::ReadFileToString(desktop_shortcut_path, &actual_contents));
  EXPECT_EQ(expected_contents, actual_contents);
}

TEST_F(WebAppShortcutLinuxTest, GetShortcutLocations) {
  // No existing shortcuts.
  {
    ShortcutLocations locations;
    std::vector<base::FilePath> result = GetShortcutLocations(
        /*env=*/nullptr, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 0u);
  }

  // Shortcut on desktop.
  {
    base::FilePath desktop_shortcut_path =
        CreateShortcutInPath(GetDesktopPath());

    ShortcutLocations locations;
    locations.on_desktop = true;

    EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
    std::vector<base::FilePath> result = GetShortcutLocations(
        /*env=*/nullptr, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], desktop_shortcut_path);
  }

  // Shortcut in autostart directory.
  {
    base::FilePath autostart_shortcut_path =
        CreateShortcutInPath(GetAutostartPath());

    ShortcutLocations locations;
    locations.in_startup = true;

    EXPECT_TRUE(base::PathExists(autostart_shortcut_path));
    std::vector<base::FilePath> result = GetShortcutLocations(
        /*env=*/nullptr, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], autostart_shortcut_path);
  }

  // Shortcut on desktop, and in autostart folder.
  {
    base::FilePath desktop_shortcut_path =
        CreateShortcutInPath(GetDesktopPath());
    base::FilePath autostart_shortcut_path =
        CreateShortcutInPath(GetAutostartPath());
    ShortcutLocations locations;
    locations.in_startup = true;
    locations.on_desktop = true;

    EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
    EXPECT_TRUE(base::PathExists(autostart_shortcut_path));
    std::vector<base::FilePath> result = GetShortcutLocations(
        /*env=*/nullptr, locations, GetProfilePath(), GetAppId());
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], desktop_shortcut_path);
    EXPECT_EQ(result[1], autostart_shortcut_path);
  }
}

// Validates Shortcut Menu actions are only added to the applications folder
// .desktop file.
TEST_F(WebAppShortcutLinuxTest, CreateDesktopShortcutWithShortcutMenuActions) {
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
            argv, exit_code, GetApplicationsPath(), invoke_count);

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

  EXPECT_TRUE(
      CreateDesktopShortcut(/*env=*/nullptr, *shortcut_info, locations));
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
        GetDesktopPath().Append(GetTemplateFilename());
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
        GetAutostartPath().Append(GetTemplateFilename());
    ASSERT_TRUE(base::PathExists(autostart_shortcut_path));

    std::string actual_contents;
    ASSERT_TRUE(
        base::ReadFileToString(autostart_shortcut_path, &actual_contents));
    EXPECT_EQ(expected_contents, actual_contents);
  }
}

}  // namespace web_app
