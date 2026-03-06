// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/isolated_browser_support.h"

#include <objbase.h>

#include "base/command_line.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class IsolatedBrowserSupportTestBase : public ::testing::Test {
 public:
  IsolatedBrowserSupportTestBase() { rom_.OverrideRegistry(HKEY_CURRENT_USER); }

 protected:
  content::BrowserTaskEnvironment env_;

 private:
  registry_util::RegistryOverrideManager rom_;
};

class IsolatedBrowserSupportTest : public IsolatedBrowserSupportTestBase,
                                   public ::testing::WithParamInterface<bool> {
};

TEST_P(IsolatedBrowserSupportTest, SetState) {
  const auto IsSystemInstall = GetParam;

  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/IsSystemInstall());
  EXPECT_FALSE(IsIsolationEnabled());

  base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
  SetIsolationState(IsolationState::kProcessIsolation, future.GetCallback());
  const auto result = future.Take();
  if (IsSystemInstall()) {
    EXPECT_THAT(result, base::test::ValueIs(IsolationState::kProcessIsolation));
    EXPECT_TRUE(IsIsolationEnabled());
  } else {
    EXPECT_THAT(result, base::test::ErrorIs(E_NOTIMPL));
    EXPECT_FALSE(IsIsolationEnabled());
  }
}

INSTANTIATE_TEST_SUITE_P(, IsolatedBrowserSupportTest, ::testing::Bool());

using IsolatedBrowserSupportSystemTest = IsolatedBrowserSupportTestBase;

TEST_F(IsolatedBrowserSupportSystemTest, CommandLine) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  base::test::TestFuture<base::expected<IsolationState, HRESULT>> future;
  SetIsolationState(IsolationState::kProcessIsolation, future.GetCallback());
  const auto result = future.Take();

  EXPECT_THAT(result, base::test::ValueIs(IsolationState::kProcessIsolation));
  EXPECT_TRUE(IsIsolationEnabled());

  // Verify isolation is never triggered if the --isolated switch is present.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kIsolated);
  EXPECT_FALSE(IsIsolationEnabled(&command_line));
}

}  // namespace chrome
