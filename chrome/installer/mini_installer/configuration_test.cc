// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/mini_installer/configuration.h"

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "base/environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

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
    EXPECT_TRUE(Initialize(::GetModuleHandle(nullptr)));
    EXPECT_TRUE(ParseCommandLine(command_line));
  }

  TestConfiguration(const TestConfiguration&) = delete;
  TestConfiguration& operator=(const TestConfiguration&) = delete;
};

}  // namespace

class MiniInstallerConfigurationTest : public ::testing::Test {
 protected:
  MiniInstallerConfigurationTest() = default;

  MiniInstallerConfigurationTest(const MiniInstallerConfigurationTest&) =
      delete;
  MiniInstallerConfigurationTest& operator=(
      const MiniInstallerConfigurationTest&) = delete;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

 private:
  registry_util::RegistryOverrideManager registry_overrides_;
};

TEST_F(MiniInstallerConfigurationTest, Program) {
  EXPECT_EQ(nullptr, mini_installer::Configuration().program());
  EXPECT_TRUE(std::wstring(L"spam.exe") ==
              TestConfiguration(L"spam.exe").program());
  EXPECT_TRUE(std::wstring(L"spam.exe") ==
              TestConfiguration(L"spam.exe --with args").program());
  EXPECT_TRUE(std::wstring(L"c:\\blaz\\spam.exe") ==
              TestConfiguration(L"c:\\blaz\\spam.exe --with args").program());
}

TEST_F(MiniInstallerConfigurationTest, ArgumentCount) {
  EXPECT_EQ(1, TestConfiguration(L"spam.exe").argument_count());
  EXPECT_EQ(2, TestConfiguration(L"spam.exe --foo").argument_count());
  EXPECT_EQ(3, TestConfiguration(L"spam.exe --foo --bar").argument_count());
}

TEST_F(MiniInstallerConfigurationTest, CommandLine) {
  static const wchar_t* const kCommandLines[] = {
      L"",
      L"spam.exe",
      L"spam.exe --foo",
  };
  for (size_t i = 0; i < _countof(kCommandLines); ++i) {
    EXPECT_TRUE(std::wstring(kCommandLines[i]) ==
                TestConfiguration(kCommandLines[i]).command_line());
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(MiniInstallerConfigurationTest, ChromeAppGuid) {
  EXPECT_STREQ(google_update::kAppGuid,
               TestConfiguration(L"spam.exe").chrome_app_guid());
  EXPECT_STREQ(google_update::kBetaAppGuid,
               TestConfiguration(L"spam.exe --chrome-beta").chrome_app_guid());
  EXPECT_STREQ(google_update::kDevAppGuid,
               TestConfiguration(L"spam.exe --chrome-dev").chrome_app_guid());
  EXPECT_STREQ(google_update::kSxSAppGuid,
               TestConfiguration(L"spam.exe --chrome-sxs").chrome_app_guid());
}
#endif

TEST_F(MiniInstallerConfigurationTest, IsSystemLevel) {
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

TEST_F(MiniInstallerConfigurationTest, HasInvalidSwitch) {
  EXPECT_FALSE(TestConfiguration(L"spam.exe").has_invalid_switch());
  EXPECT_TRUE(
      TestConfiguration(L"spam.exe --chrome-frame").has_invalid_switch());
  EXPECT_TRUE(TestConfiguration(L"spam.exe --cleanup").has_invalid_switch());
}

TEST_F(MiniInstallerConfigurationTest, DeleteExtractedFilesDefaultTrue) {
  EXPECT_TRUE(TestConfiguration(L"spam.exe").should_delete_extracted_files());
}

TEST_F(MiniInstallerConfigurationTest, DeleteExtractedFilesFalse) {
  ASSERT_EQ(
      base::win::RegKey(HKEY_CURRENT_USER, kCleanupRegistryKey, KEY_SET_VALUE)
          .WriteValue(kCleanupRegistryValue, L"0"),
      ERROR_SUCCESS);
  EXPECT_FALSE(TestConfiguration(L"spam.exe").should_delete_extracted_files());
}

TEST_F(MiniInstallerConfigurationTest, DeleteExtractedFilesBogusValues) {
  ASSERT_EQ(
      base::win::RegKey(HKEY_CURRENT_USER, kCleanupRegistryKey, KEY_SET_VALUE)
          .WriteValue(kCleanupRegistryValue, L""),
      ERROR_SUCCESS);
  EXPECT_TRUE(TestConfiguration(L"spam.exe").should_delete_extracted_files());
  ASSERT_EQ(
      base::win::RegKey(HKEY_CURRENT_USER, kCleanupRegistryKey, KEY_SET_VALUE)
          .WriteValue(kCleanupRegistryValue, L"1"),
      ERROR_SUCCESS);
  EXPECT_TRUE(TestConfiguration(L"spam.exe").should_delete_extracted_files());
  ASSERT_EQ(
      base::win::RegKey(HKEY_CURRENT_USER, kCleanupRegistryKey, KEY_SET_VALUE)
          .WriteValue(kCleanupRegistryValue, L"hello"),
      ERROR_SUCCESS);
  EXPECT_TRUE(TestConfiguration(L"spam.exe").should_delete_extracted_files());
}

}  // namespace mini_installer
