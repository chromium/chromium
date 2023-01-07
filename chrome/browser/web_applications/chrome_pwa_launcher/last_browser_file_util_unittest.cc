// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/last_browser_file_util.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(WebAppLauncherTest, WriteChromePathToLastBrowserFile) {
  // Redirect |DIR_USER_DATA| to a temporary directory during testing.
  base::ScopedPathOverride user_data_dir_override(chrome::DIR_USER_DATA);

  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  ASSERT_FALSE(user_data_dir.empty());

  // Write current executable path to the Last Browser file.
  WriteChromePathToLastBrowserFile(user_data_dir);

  // The Last Browser file should contain the current executable path.
  base::FilePath current_path;
  EXPECT_TRUE(base::PathService::Get(base::FILE_EXE, &current_path));
  EXPECT_EQ(
      ReadChromePathFromLastBrowserFile(user_data_dir.Append(L"Last Browser")),
      current_path);
}

// GetLastBrowserFileFromWebAppDir() assumes that Progressive Web App launchers
// calling it are located in "User Data/<profile>/Web Applications/<app ID>/",
// and simply looks for the Last Browser file in the web-app directory's
// great-grandparent User Data directory. This tests that assumption by checking
// that the <app ID> directory is two hops below the <profile> directory.
TEST(WebAppLauncherTest, WebAppDirHierarchyAsExpected) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  const base::FilePath profile_dir = profile.GetPath();
  const base::FilePath web_app_dir =
      GetOsIntegrationResourcesDirectoryForApp(profile_dir, "test", GURL());
  EXPECT_EQ(web_app_dir.DirName().DirName(), profile_dir);
}

}  // namespace web_app
