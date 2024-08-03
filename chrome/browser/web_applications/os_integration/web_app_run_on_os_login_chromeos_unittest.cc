// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
TEST(WebAppRunOnOsLoginChromeOsTest, Register) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // On ChromeOS, Register does nothing and can just succeed. This behavior is
  // checked in the browser test for WebAppRunOnOsLoginManager, as the result of
  // Register affects InstallOsHooks.
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  base::test::TestFuture<Result> result;
  internals::RegisterRunOnOsLogin(*shortcut_info, result.GetCallback());
  EXPECT_EQ(result.Get(), Result::kOk);
}

TEST(WebAppRunOnOsLoginChromeOsTest, Unregister) {
  // On ChromeOS, Unregister does nothing and can just succeed.  This behavior
  // is checked in the browser test for WebAppRunOnOsLoginManager, as the result
  // of Register affects InstallOsHooks.
  Result result = internals::UnregisterRunOnOsLogin("", base::FilePath(), u"");
  EXPECT_EQ(Result::kOk, result);
}
}  // namespace

}  // namespace web_app
