// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcut.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(WebAppShortcutTest, AppDirWithId) {
  base::FilePath profile_path(FILE_PATH_LITERAL("profile"));
  base::FilePath result(GetWebAppDataDirectory(profile_path, "123", GURL()));
  base::FilePath expected =
      profile_path.AppendASCII("Web Applications").AppendASCII("_crx_123");
  EXPECT_EQ(expected, result);
}

TEST(WebAppShortcutTest, AppDirWithUrl) {
  base::FilePath profile_path(FILE_PATH_LITERAL("profile"));
  base::FilePath result(GetWebAppDataDirectory(profile_path, std::string(),
                                               GURL("http://example.com")));
  base::FilePath expected = profile_path.AppendASCII("Web Applications")
                                .AppendASCII("example.com")
                                .AppendASCII("http_80");
  EXPECT_EQ(expected, result);
}

}  // namespace web_app
