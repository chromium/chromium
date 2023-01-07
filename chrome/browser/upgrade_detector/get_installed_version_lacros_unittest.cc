// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

class GetInstalledVersionLacrosTest : public ::testing::Test {
 public:
  GetInstalledVersionLacrosTest() = default;
  ~GetInstalledVersionLacrosTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

// Tests that if no Ash BrowserVersionService is available, the default
// installed version returned matches the currently running browser version.
TEST_F(GetInstalledVersionLacrosTest,
       NoBrowserVersionServiceAvailableInstallVersionDefaultsToRunningVersion) {
  base::RunLoop run_loop;
  base::MockCallback<InstalledVersionCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&run_loop](InstalledAndCriticalVersion versions) {
        EXPECT_EQ(version_info::GetVersion(), versions.installed_version);
        EXPECT_FALSE(versions.critical_version.has_value());
        run_loop.Quit();
      });
  GetInstalledVersion(callback.Get());
  run_loop.Run();
}
