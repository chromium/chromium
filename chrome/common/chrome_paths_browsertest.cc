// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace chrome {

using ChromePathsBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromePathsBrowserTest, DefaultUserDataDirectory) {
  // The browser test harness overrides the user data directory using the
  // --user-data-dir argument.
  EXPECT_FALSE(IsUsingDefaultDataDirectory().value());

  SetUsingDefaultUserDataDirectoryForTesting(true);
  EXPECT_TRUE(IsUsingDefaultDataDirectory().value());

  SetUsingDefaultUserDataDirectoryForTesting(std::nullopt);
  EXPECT_FALSE(IsUsingDefaultDataDirectory().value());
}

}  // namespace chrome
