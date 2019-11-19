// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcut_linux.h"

#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Provides mock environment variables values based on a stored map.
class MockEnvironment : public base::Environment {
 public:
  MockEnvironment() {}

  void Set(base::StringPiece name, const std::string& value) {
    variables_[name.as_string()] = value;
  }

  bool GetVar(base::StringPiece variable_name, std::string* result) override {
    if (base::Contains(variables_, variable_name.as_string())) {
      *result = variables_[variable_name.as_string()];
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

  DISALLOW_COPY_AND_ASSIGN(MockEnvironment);
};

bool WriteEmptyFile(const base::FilePath& path) {
  return base::WriteFile(path, "", 0) == 0;
}

bool WriteString(const base::FilePath& path, const base::StringPiece& str) {
  int bytes_written = base::WriteFile(path, str.data(), str.size());
  if (bytes_written < 0)
    return false;

  return static_cast<size_t>(bytes_written) == str.size();
}

}  // namespace

TEST(ShellIntegrationTest, GetExistingShortcutLocations) {
  base::FilePath kProfilePath("Profile 1");
  const char kExtensionId[] = "test_extension";
  const char kTemplateFilename[] = "chrome-test_extension-Profile_1.desktop";
  base::FilePath kTemplateFilepath(kTemplateFilename);
  const char kNoDisplayDesktopFile[] = "[Desktop Entry]\nNoDisplay=true";

  content::BrowserTaskEnvironment task_environment;

  // No existing shortcuts.
  {
    MockEnvironment env;
    ShortcutLocations result =
        GetExistingShortcutLocations(&env, kProfilePath, kExtensionId);
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);

    EXPECT_FALSE(result.in_quick_launch_bar);
  }

  // Shortcut on desktop.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath desktop_path = temp_dir.GetPath();

    MockEnvironment env;
    ASSERT_TRUE(base::CreateDirectory(desktop_path));
    ASSERT_TRUE(WriteEmptyFile(desktop_path.Append(kTemplateFilename)));
    ShortcutLocations result = GetExistingShortcutLocations(
        &env, kProfilePath, kExtensionId, desktop_path);
    EXPECT_TRUE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_NONE, result.applications_menu_location);

    EXPECT_FALSE(result.in_quick_launch_bar);
  }

  // Shortcut in applications directory.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath apps_path = temp_dir.GetPath().Append("applications");

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.GetPath().value());
    ASSERT_TRUE(base::CreateDirectory(apps_path));
    ASSERT_TRUE(WriteEmptyFile(apps_path.Append(kTemplateFilename)));
    ShortcutLocations result =
        GetExistingShortcutLocations(&env, kProfilePath, kExtensionId);
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
              result.applications_menu_location);

    EXPECT_FALSE(result.in_quick_launch_bar);
  }

  // Shortcut in applications directory with NoDisplay=true.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath apps_path = temp_dir.GetPath().Append("applications");

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.GetPath().value());
    ASSERT_TRUE(base::CreateDirectory(apps_path));
    ASSERT_TRUE(WriteString(apps_path.Append(kTemplateFilename),
                            kNoDisplayDesktopFile));
    ShortcutLocations result =
        GetExistingShortcutLocations(&env, kProfilePath, kExtensionId);
    // Doesn't count as being in applications menu.
    EXPECT_FALSE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_HIDDEN, result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
  }

  // Shortcut on desktop and in applications directory.
  {
    base::ScopedTempDir temp_dir1;
    ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
    base::FilePath desktop_path = temp_dir1.GetPath();

    base::ScopedTempDir temp_dir2;
    ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
    base::FilePath apps_path = temp_dir2.GetPath().Append("applications");

    MockEnvironment env;
    ASSERT_TRUE(base::CreateDirectory(desktop_path));
    ASSERT_TRUE(WriteEmptyFile(desktop_path.Append(kTemplateFilename)));
    env.Set("XDG_DATA_HOME", temp_dir2.GetPath().value());
    ASSERT_TRUE(base::CreateDirectory(apps_path));
    ASSERT_TRUE(WriteEmptyFile(apps_path.Append(kTemplateFilename)));
    ShortcutLocations result = GetExistingShortcutLocations(
        &env, kProfilePath, kExtensionId, desktop_path);
    EXPECT_TRUE(result.on_desktop);
    EXPECT_EQ(APP_MENU_LOCATION_SUBDIR_CHROMEAPPS,
              result.applications_menu_location);
    EXPECT_FALSE(result.in_quick_launch_bar);
  }
}

TEST(ShellIntegrationTest, GetExtensionShortcutFilename) {
  base::FilePath kProfilePath("a/b/c/Profile Name?");
  const char kExtensionId[] = "extensionid";
  EXPECT_EQ(base::FilePath("chrome-extensionid-Profile_Name_.desktop"),
            GetAppShortcutFilename(kProfilePath, kExtensionId));
}

}  // namespace web_app
