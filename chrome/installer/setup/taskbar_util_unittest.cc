// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/taskbar_util.h"

#include "base/test/test_reg_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test SetInstallerPinnedChromeToTaskbar and GetInstallerPinnedChromeToTaskbar.
TEST(TaskbarUtilTest, InstallerPinnedChromeToTaskbar) {
  registry_util::RegistryOverrideManager registry_override_manager;
  // Override the registry so that tests can freely push state to it.
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager.OverrideRegistry(HKEY_CURRENT_USER));

  // By default, installer has not pinned chrome to taskbar.
  EXPECT_FALSE(GetInstallerPinnedChromeToTaskbar().has_value());

  // Verify that GetInstallerPinnedChromeToTaskbar returns the values
  // set by SetInstallerPinnedChromeToTaskbar.
  ASSERT_TRUE(SetInstallerPinnedChromeToTaskbar(true));
  EXPECT_TRUE(GetInstallerPinnedChromeToTaskbar().value());
  ASSERT_TRUE(SetInstallerPinnedChromeToTaskbar(false));
  EXPECT_FALSE(GetInstallerPinnedChromeToTaskbar().value());
}
