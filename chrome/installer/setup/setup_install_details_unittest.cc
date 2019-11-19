// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_install_details.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/test_reg_util_win.h"
#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/installer/util/master_preferences.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

struct TestData {
  // Inputs:
  const wchar_t* command_line;
  const wchar_t* uninstall_args;
  const wchar_t* product_ap;
  const wchar_t* binaries_ap;

  // Expectations:
  install_static::InstallConstantIndex index;
  bool system_level;
  const wchar_t* channel;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr TestData kTestData[] = {
    // User-level test cases.
    {
        L"setup.exe",                  // User-level, primary mode.
        L"",                           // New install.
        L"x64-stable",                 // Stable channel.
        L"1.1-beta",                   // Value ignored in binaries.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"",                           // Expect stable channel.
    },
    {
        L"setup.exe",                  // User-level, primary mode.
        L"--uninstall",                // Updating an existing install.
        L"x64-stable",                 // Stable channel.
        L"1.1-beta",                   // Value ignored in binaries.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"",                           // Expect stable channel.
    },
    {
        L"setup.exe",                    // User-level, primary mode.
        L"--uninstall --multi-install",  // Updating an existing multi-install.
        L"x64-stable",                   // Value ignored in product.
        L"1.1-beta",                     // Channel read from binaries.
        install_static::STABLE_INDEX,    // Expect primary mode.
        false,                           // Expect user-level.
        L"beta",                         // Expect beta channel.
    },
    {
        L"setup.exe",                  // User-level, primary mode.
        L"",                           // New install.
        L"1.1-beta",                   // Beta channel.
        L"x64-stable",                 // Value ignored in binaries.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"beta",                       // Expect beta channel.
    },
    {
        L"setup.exe --chrome-beta",  // User-level, secondary SxS beta mode.
        L"",                         // New install.
        L"",                         // Unused.
        L"",                         // Unused.
        install_static::BETA_INDEX,  // Expect SxS beta mode.
        false,                       // Expect user-level.
        L"beta",                     // Expect beta channel.
    },
    {
        L"setup.exe --chrome-beta",    // User-level, secondary SxS beta mode.
        L"--uninstall --chrome-beta",  // Update.
        L"",                           // Unused.
        L"",                           // Unused.
        install_static::BETA_INDEX,    // Expect SxS beta mode.
        false,                         // Expect user-level.
        L"beta",                       // Expect beta channel.
    },
    {
        L"setup.exe --chrome-dev",  // User-level, secondary SxS dev mode.
        L"",                        // New install.
        L"",                        // Unused.
        L"",                        // Unused.
        install_static::DEV_INDEX,  // Expect SxS dev mode.
        false,                      // Expect user-level.
        L"dev",                     // Expect dev channel.
    },
    {
        L"setup.exe --chrome-dev",    // User-level, secondary SxS dev mode.
        L"--uninstall --chrome-dev",  // Update.
        L"",                          // Unused.
        L"",                          // Unused.
        install_static::DEV_INDEX,    // Expect SxS dev mode.
        false,                        // Expect user-level.
        L"dev",                       // Expect dev channel.
    },
    {
        L"setup.exe --chrome-sxs",     // User-level, secondary SxS canary mode.
        L"",                           // New install.
        L"",                           // Unused.
        L"",                           // Unused.
        install_static::CANARY_INDEX,  // Expect SxS canary mode.
        false,                         // Expect user-level.
        L"canary",                     // Expect canary channel.
    },
    {
        L"setup.exe --chrome-sxs",     // User-level, secondary SxS canary mode.
        L"--uninstall --chrome-sxs",   // Update.
        L"",                           // Unused.
        L"",                           // Unused.
        install_static::CANARY_INDEX,  // Expect SxS canary mode.
        false,                         // Expect user-level.
        L"canary",                     // Expect canary channel.
    },

    // System-level test cases.
    {
        L"setup.exe --system-level",   // System-level, primary mode.
        L"",                           // New install.
        L"x64-stable",                 // Stable channel.
        L"1.1-beta",                   // Value ignored in binaries.
        install_static::STABLE_INDEX,  // Expect primary mode.
        true,                          // Expect system-level.
        L"",                           // Expect stable channel.
    },
    {
        L"setup.exe --system-level",    // System-level, primary mode.
        L"--uninstall --system-level",  // Updating an existing install.
        L"x64-stable",                  // Stable channel.
        L"1.1-beta",                    // Value ignored in binaries.
        install_static::STABLE_INDEX,   // Expect primary mode.
        true,                           // Expect system-level.
        L"",                            // Expect stable channel.
    },
    {
        L"setup.exe --system-level",  // System-level, primary mode.
        // Updating an existing multi-install.
        L"--uninstall --system-level --multi-install",
        L"x64-stable",                 // Value ignored in product.
        L"1.1-beta",                   // Channel read from binaries.
        install_static::STABLE_INDEX,  // Expect primary mode.
        true,                          // Expect system-level.
        L"beta",                       // Expect beta channel.
    },
    {
        L"setup.exe --system-level",   // System-level, primary mode.
        L"",                           // New install.
        L"1.1-beta",                   // Beta channel.
        L"x64-stable",                 // Value ignored in binaries.
        install_static::STABLE_INDEX,  // Expect primary mode.
        true,                          // Expect system-level.
        L"beta",                       // Expect beta channel.
    },
    {
        L"setup.exe --system-level --chrome-beta",  // User-level, secondary SxS
                                                    // beta mode.
        L"",                                        // New install.
        L"",                                        // Unused.
        L"",                                        // Unused.
        install_static::BETA_INDEX,                 // Expect SxS beta mode.
        true,                                       // Expect user-level.
        L"beta",                                    // Expect beta channel.
    },
    {
        L"setup.exe --system-level --chrome-beta",  // User-level, secondary SxS
                                                    // beta mode.
        L"--uninstall --system-level --chrome-beta",  // Update.
        L"",                                          // Unused.
        L"",                                          // Unused.
        install_static::BETA_INDEX,                   // Expect SxS beta mode.
        true,                                         // Expect user-level.
        L"beta",                                      // Expect beta channel.
    },
    {
        L"setup.exe --system-level --chrome-dev",  // User-level, secondary SxS
                                                   // dev mode.
        L"",                                       // New install.
        L"",                                       // Unused.
        L"",                                       // Unused.
        install_static::DEV_INDEX,                 // Expect SxS dev mode.
        true,                                      // Expect user-level.
        L"dev",                                    // Expect dev channel.
    },
    {
        L"setup.exe --system-level --chrome-dev",  // User-level, secondary SxS
                                                   // dev mode.
        L"--uninstall --system-level --chrome-dev",  // Update.
        L"",                                         // Unused.
        L"",                                         // Unused.
        install_static::DEV_INDEX,                   // Expect SxS dev mode.
        true,                                        // Expect user-level.
        L"dev",                                      // Expect dev channel.
    },
};
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr TestData kTestData[] = {
    // User-level test cases.
    {
        L"setup.exe",                    // User-level, primary mode.
        L"",                             // New install.
        L"",                             // Channels are not supported.
        L"",                             // Channels are not supported.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        false,                           // Expect user-level.
        L"",                             // Expect empty channel.
    },
    {
        L"setup.exe",                    // User-level, primary mode.
        L"--uninstall",                  // Updating an existing install.
        L"",                             // Channels are not supported.
        L"",                             // Channels are not supported.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        false,                           // Expect user-level.
        L"",                             // Expect empty channel.
    },

    // System-level test cases.
    {
        L"setup.exe --system-level",     // System-level, primary mode.
        L"",                             // New install.
        L"",                             // Channels are not supported.
        L"",                             // Channels are not supported.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        true,                            // Expect system-level.
        L"",                             // Expect empty channel.
    },
    {
        L"setup.exe --system-level",     // System-level, primary mode.
        L"--uninstall --system-level",   // Updating an existing install.
        L"",                             // Channels are not supported.
        L"",                             // Channels are not supported.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        true,                            // Expect system-level.
        L"",                             // Expect empty channel.
    },
};
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

class MakeInstallDetailsTest : public testing::TestWithParam<TestData> {
 protected:
  MakeInstallDetailsTest()
      : test_data_(GetParam()),
        root_key_(test_data_.system_level ? HKEY_LOCAL_MACHINE
                                          : HKEY_CURRENT_USER),
        nt_root_key_(test_data_.system_level ? nt::HKLM : nt::HKCU),
        command_line_(base::CommandLine::NO_PROGRAM) {
    // Prepare the inputs from the process command line.
    command_line_.ParseFromString(test_data_.command_line);
    master_preferences_ =
        std::make_unique<installer::MasterPreferences>(command_line_);
  }

  void SetUp() override {
    base::string16 path;
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(root_key_, &path));
    nt::SetTestingOverride(nt_root_key_, path);

    // Prepare the inputs from the machine's state.
    ASSERT_NO_FATAL_FAILURE(SetUninstallArguments(
        root_key_, install_static::kInstallModes[test_data_.index].app_guid,
        test_data_.uninstall_args));
    if (install_static::kUseGoogleUpdateIntegration) {
      ASSERT_NO_FATAL_FAILURE(SetProductAp(
          root_key_, install_static::kInstallModes[test_data_.index].app_guid,
          test_data_.product_ap));
      ASSERT_NO_FATAL_FAILURE(SetBinariesAp(root_key_, test_data_.binaries_ap));
    }
  }

  void TearDown() override {
    nt::SetTestingOverride(nt_root_key_, base::string16());
  }

  const TestData& test_data() const { return test_data_; }

  const base::CommandLine& command_line() const { return command_line_; }

  const installer::MasterPreferences& master_preferences() const {
    return *master_preferences_;
  }

 private:
  static void SetUninstallArguments(HKEY root_key,
                                    const wchar_t* app_guid,
                                    const wchar_t* uninstall_args) {
    // Do nothing if there's no value to write.
    if (!uninstall_args || !*uninstall_args)
      return;
    // Make it appear that the product is installed with the given uninstall
    // args.
    ASSERT_THAT(
        base::win::RegKey(root_key,
                          install_static::GetClientsKeyPath(app_guid).c_str(),
                          KEY_WOW64_32KEY | KEY_SET_VALUE)
            .WriteValue(L"pv", L"1.2.3.4"),
        Eq(ERROR_SUCCESS));
    ASSERT_THAT(
        base::win::RegKey(
            root_key, install_static::GetClientStateKeyPath(app_guid).c_str(),
            KEY_WOW64_32KEY | KEY_SET_VALUE)
            .WriteValue(L"UninstallArguments", uninstall_args),
        Eq(ERROR_SUCCESS));
  }

  static void SetProductAp(HKEY root_key,
                           const wchar_t* app_guid,
                           const wchar_t* ap) {
    // Do nothing if there's no value to write.
    if (!ap || !*ap)
      return;
    ASSERT_THAT(
        base::win::RegKey(
            root_key, install_static::GetClientStateKeyPath(app_guid).c_str(),
            KEY_WOW64_32KEY | KEY_SET_VALUE)
            .WriteValue(L"ap", ap),
        Eq(ERROR_SUCCESS));
  }

  static void SetBinariesAp(HKEY root_key, const wchar_t* ap) {
    // Do nothing if there's no value to write.
    if (!ap || !*ap)
      return;
    ASSERT_THAT(
        base::win::RegKey(
            root_key, install_static::GetBinariesClientStateKeyPath().c_str(),
            KEY_WOW64_32KEY | KEY_SET_VALUE)
            .WriteValue(L"ap", ap),
        Eq(ERROR_SUCCESS));
  }

  registry_util::RegistryOverrideManager override_manager_;
  const TestData& test_data_;
  HKEY root_key_;
  nt::ROOT_KEY nt_root_key_;
  base::CommandLine command_line_;
  std::unique_ptr<installer::MasterPreferences> master_preferences_;

  DISALLOW_COPY_AND_ASSIGN(MakeInstallDetailsTest);
};

TEST_P(MakeInstallDetailsTest, Test) {
  std::unique_ptr<install_static::PrimaryInstallDetails> details(
      MakeInstallDetails(command_line(), master_preferences()));
  EXPECT_THAT(details->install_mode_index(), Eq(test_data().index));
  EXPECT_THAT(details->system_level(), Eq(test_data().system_level));
  EXPECT_THAT(details->channel(), Eq(test_data().channel));
}

INSTANTIATE_TEST_SUITE_P(All,
                         MakeInstallDetailsTest,
                         testing::ValuesIn(kTestData));
