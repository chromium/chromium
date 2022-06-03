// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/configuration.h"

#include <stddef.h>
#include <stdlib.h>

#include <memory>

#include "base/environment.h"
#include "base/test/test_reg_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

// A helper class to set the "GoogleUpdateIsMachine" environment variable.
class ScopedGoogleUpdateIsMachine {
 public:
  explicit ScopedGoogleUpdateIsMachine(bool value)
      : env_(base::Environment::Create()) {
    env_->SetVar("GoogleUpdateIsMachine", value ? "1" : "0");
  }

  ~ScopedGoogleUpdateIsMachine() { env_->UnSetVar("GoogleUpdateIsMachine"); }

 private:
  std::unique_ptr<base::Environment> env_;
};

class TestConfiguration : public Configuration {
 public:
  explicit TestConfiguration(const wchar_t* command_line) {
    EXPECT_TRUE(ParseCommandLine(command_line));
  }
  TestConfiguration(const TestConfiguration&) = delete;
  TestConfiguration& operator=(const TestConfiguration&) = delete;
};

}  // namespace

class UpdaterInstallerConfigurationTest : public ::testing::Test {
 protected:
  UpdaterInstallerConfigurationTest() = default;
  UpdaterInstallerConfigurationTest(const UpdaterInstallerConfigurationTest&) =
      delete;
  UpdaterInstallerConfigurationTest& operator=(
      const UpdaterInstallerConfigurationTest&) = delete;
};

// Test that the operation type is CLEANUP iff --cleanup is on the cmdline.
TEST_F(UpdaterInstallerConfigurationTest, Operation) {
  EXPECT_EQ(Configuration::INSTALL_PRODUCT,
            TestConfiguration(L"spam.exe").operation());
  EXPECT_EQ(Configuration::INSTALL_PRODUCT,
            TestConfiguration(L"spam.exe --clean").operation());
  EXPECT_EQ(Configuration::INSTALL_PRODUCT,
            TestConfiguration(L"spam.exe --cleanupthis").operation());

  EXPECT_EQ(Configuration::CLEANUP,
            TestConfiguration(L"spam.exe --cleanup").operation());
  EXPECT_EQ(Configuration::CLEANUP,
            TestConfiguration(L"spam.exe --cleanup now").operation());
}

TEST_F(UpdaterInstallerConfigurationTest, IsSystemLevel) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").is_system_level());
  EXPECT_FALSE(TestConfiguration(L"spam.exe --chrome").is_system_level());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --system-level").is_system_level());

  {
    ScopedGoogleUpdateIsMachine env_setter(false);
    EXPECT_FALSE(TestConfiguration(L"spam.exe").is_system_level());
  }

  {
    ScopedGoogleUpdateIsMachine env_setter(true);
    EXPECT_TRUE(TestConfiguration(L"spam.exe").is_system_level());
  }
}

TEST_F(UpdaterInstallerConfigurationTest, HasInvalidSwitch) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").has_invalid_switch());
  EXPECT_TRUE(
      TestConfiguration(L"spam.exe --chrome-frame").has_invalid_switch());
}

}  // namespace updater
