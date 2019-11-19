// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/configuration.h"

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "base/environment.h"
#include "base/macros.h"
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

  ~ScopedGoogleUpdateIsMachine() {
    env_->UnSetVar("GoogleUpdateIsMachine");
  }

 private:
  std::unique_ptr<base::Environment> env_;
};

class TestConfiguration : public Configuration {
 public:
  explicit TestConfiguration(const wchar_t* command_line) {
    EXPECT_TRUE(ParseCommandLine(command_line));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConfiguration);
};

}  // namespace

class MiniInstallerConfigurationTest : public ::testing::Test {
 protected:
  MiniInstallerConfigurationTest() = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  // Adds sufficient state in the registry for Configuration to think that
  // Chrome is already installed at |system_level| as per |multi_install|.
  void AddChromeRegistryState(bool system_level, bool multi_install) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    static constexpr wchar_t kClientsPath[] =
        L"SOFTWARE\\Google\\Update\\Clients\\"
        L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
    static constexpr wchar_t kClientStatePath[] =
        L"SOFTWARE\\Google\\Update\\ClientState\\"
        L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    static constexpr wchar_t kClientsPath[] = L"SOFTWARE\\Chromium";
    static constexpr wchar_t kClientStatePath[] = L"SOFTWARE\\Chromium";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    base::string16 uninstall_arguments(L"--uninstall");
    if (system_level)
      uninstall_arguments += L" --system_level";
    if (multi_install)
      uninstall_arguments += L" --multi-install --chrome";
    const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    // Use base::win::RegKey rather than mini_installer's since it's more
    // prevalent in the codebase and more likely to be easy to understand.
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(root, kClientsPath, KEY_WOW64_32KEY | KEY_SET_VALUE));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"pv", L"4.3.2.1"));
    ASSERT_EQ(ERROR_SUCCESS, key.Create(root, kClientStatePath,
                                        KEY_WOW64_32KEY | KEY_SET_VALUE));
    ASSERT_EQ(
        ERROR_SUCCESS,
        key.WriteValue(L"UninstallArguments", uninstall_arguments.c_str()));
  }

 private:
  registry_util::RegistryOverrideManager registry_overrides_;

  DISALLOW_COPY_AND_ASSIGN(MiniInstallerConfigurationTest);
};

// Test that the operation type is CLEANUP iff --cleanup is on the cmdline.
TEST_F(MiniInstallerConfigurationTest, Operation) {
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

TEST_F(MiniInstallerConfigurationTest, Program) {
  EXPECT_TRUE(NULL == mini_installer::Configuration().program());
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

TEST_F(MiniInstallerConfigurationTest, IsUpdatingUserSingle) {
  AddChromeRegistryState(false /* !system_level */, false /* !multi_install */);
  EXPECT_FALSE(TestConfiguration(L"spam.exe").is_updating_multi_chrome());
}

TEST_F(MiniInstallerConfigurationTest, IsUpdatingSystemSingle) {
  AddChromeRegistryState(true /* system_level */, false /* !multi_install */);
  EXPECT_FALSE(
      TestConfiguration(L"spam.exe --system-level").is_updating_multi_chrome());
}

TEST_F(MiniInstallerConfigurationTest, IsUpdatingUserMulti) {
  AddChromeRegistryState(false /* !system_level */, true /* multi_install */);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(TestConfiguration(L"spam.exe").is_updating_multi_chrome());
#else
  EXPECT_FALSE(TestConfiguration(L"spam.exe").is_updating_multi_chrome());
#endif
}

TEST_F(MiniInstallerConfigurationTest, IsUpdatingSystemMulti) {
  AddChromeRegistryState(true /* system_level */, true /* multi_install */);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(
      TestConfiguration(L"spam.exe --system-level").is_updating_multi_chrome());
#else
  EXPECT_FALSE(
      TestConfiguration(L"spam.exe --system-level").is_updating_multi_chrome());
#endif
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
  EXPECT_TRUE(TestConfiguration(L"spam.exe --chrome-frame")
                  .has_invalid_switch());
}

}  // namespace mini_installer
