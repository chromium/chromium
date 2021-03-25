// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WallpaperControllerClientImplTest : public testing::Test {
 public:
  WallpaperControllerClientImplTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~WallpaperControllerClientImplTest() override = default;

 private:
  ScopedTestingLocalState local_state_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerClientImplTest);
};

TEST_F(WallpaperControllerClientImplTest, Construction) {
  TestWallpaperController controller;
  WallpaperControllerClientImpl client;
  client.InitForTesting(&controller);

  // Singleton was initialized.
  EXPECT_EQ(&client, WallpaperControllerClientImpl::Get());

  // Object was set as client.
  EXPECT_TRUE(controller.was_client_set());
}

}  // namespace
