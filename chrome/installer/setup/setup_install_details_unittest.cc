// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/setup/setup_install_details.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/memory/raw_ref.h"
#include "base/test/test_reg_util_win.h"
#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/util/initial_preferences.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

struct TestData {
  // Inputs:
  const wchar_t* command_line;
  const wchar_t* uninstall_args;

  // Expectations:
  install_static::InstallConstantIndex index;
  bool system_level;
  const wchar_t* channel;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  bool is_extended_stable_channel;
  const wchar_t* channel_override;
#endif
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr TestData kTestData[] = {
    // User-level test cases.
    {
        L"setup.exe",                  // User-level, primary mode.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"",                           // Expect stable channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --channel=stable",  // User-level, primary mode.
        L"",                            // New install.
        install_static::STABLE_INDEX,   // Expect primary mode.
        false,                          // Expect user-level.
        L"",                            // Expect stable channel.
        false,                          // Expect not extended stable channel.
        L"stable",                      // Expect the channel override.
    },
    {
        L"setup.exe --channel",        // User-level, primary mode.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"",                           // Expect stable channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --channel=extended",  // User-level, primary mode.
        L"",                              // New install.
        install_static::STABLE_INDEX,     // Expect primary mode.
        false,                            // Expect user-level.
        L"",                              // Expect stable channel.
        true,                             // Expect extended stable channel.
        L"extended",                      // Expect the channel override.
    },
    {
        L"setup.exe",                  // User-level, primary mode.
        L"--uninstall",                // Updating an existing install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"",                           // Expect stable channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --channel=beta",   // User-level, primary mode, beta
                                       // channel.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"beta",                       // Expect beta channel.
        false,                         // Expect not extended stable channel.
        L"beta"                        // Expect the channel override.
    },
    {
        L"setup.exe --channel=beta",   // User-level, primary mode, beta
                                       // channel.
        L"--uninstall",                // Updating an existing install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"beta",                       // Expect beta channel.
        false,                         // Expect not extended stable channel.
        L"beta",                       // Expect the channel override.
    },
    {
        L"setup.exe --channel=dev",    // User-level, primary mode, dev channel.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"dev",                        // Expect dev channel.
        false,                         // Expect not extended stable channel.
        L"dev",                        // Expect the channel override.
    },
    {
        L"setup.exe --channel=dev",    // User-level, primary mode, dev channel.
        L"--uninstall",                // Updating an existing install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"dev",                        // Expect dev channel.
        false,                         // Expect not extended stable channel.
        L"dev",                        // Expect the channel override.
    },
    {
        L"setup.exe --channel=bad",    // User-level, primary mode, bad channel.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"",                           // Expect stable channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --channel=bad",    // User-level, primary mode, bad channel.
        L"--uninstall",                // Updating an existing install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        false,                         // Expect user-level.
        L"",                           // Expect stable channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --chrome-beta",  // User-level, secondary SxS beta mode.
        L"",                         // New install.
        install_static::BETA_INDEX,  // Expect SxS beta mode.
        false,                       // Expect user-level.
        L"beta",                     // Expect beta channel.
        false,                       // Expect not extended stable channel.
        L"",                         // Expect no channel override.
    },
    {
        L"setup.exe --chrome-beta --channel=dev",  // User-level, secondary SxS
                                                   // beta mode.
        L"",                                       // New install.
        install_static::BETA_INDEX,                // Expect SxS beta mode.
        false,                                     // Expect user-level.
        L"beta",                                   // Expect beta channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --chrome-beta --channel=dev",  // User-level, secondary SxS
                                                   // beta mode.
        L"--uninstall --chrome-beta",              // Update.
        install_static::BETA_INDEX,                // Expect SxS beta mode.
        false,                                     // Expect user-level.
        L"beta",                                   // Expect beta channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --chrome-beta",    // User-level, secondary SxS beta mode.
        L"--uninstall --chrome-beta",  // Update.
        install_static::BETA_INDEX,    // Expect SxS beta mode.
        false,                         // Expect user-level.
        L"beta",                       // Expect beta channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --chrome-dev",  // User-level, secondary SxS dev mode.
        L"",                        // New install.
        install_static::DEV_INDEX,  // Expect SxS dev mode.
        false,                      // Expect user-level.
        L"dev",                     // Expect dev channel.
        false,                      // Expect not extended stable channel.
        L"",                        // Expect no channel override.
    },
    {
        L"setup.exe --chrome-dev --channel=beta",  // User-level, secondary SxS
                                                   // dev mode.
        L"",                                       // New install.
        install_static::DEV_INDEX,                 // Expect SxS dev mode.
        false,                                     // Expect user-level.
        L"dev",                                    // Expect dev channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --chrome-dev --channel",  // User-level, secondary SxS
                                              // dev mode.
        L"--uninstall --chrome-dev",          // Update.
        install_static::DEV_INDEX,            // Expect SxS dev mode.
        false,                                // Expect user-level.
        L"dev",                               // Expect dev channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --chrome-dev",    // User-level, secondary SxS dev mode.
        L"--uninstall --chrome-dev",  // Update.
        install_static::DEV_INDEX,    // Expect SxS dev mode.
        false,                        // Expect user-level.
        L"dev",                       // Expect dev channel.
        false,                        // Expect not extended stable channel.
        L"",                          // Expect no channel override.
    },
    {
        L"setup.exe --chrome-sxs",     // User-level, secondary SxS canary mode.
        L"",                           // New install.
        install_static::CANARY_INDEX,  // Expect SxS canary mode.
        false,                         // Expect user-level.
        L"canary",                     // Expect canary channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --chrome-sxs --channel=dev",  // User-level, secondary SxS
                                                  // canary mode.
        L"",                                      // New install.
        install_static::CANARY_INDEX,             // Expect SxS canary mode.
        false,                                    // Expect user-level.
        L"canary",                                // Expect canary channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --chrome-sxs --channel",  // User-level, secondary SxS
                                              // canary mode.
        L"",                                  // New install.
        install_static::CANARY_INDEX,         // Expect SxS canary mode.
        false,                                // Expect user-level.
        L"canary",                            // Expect canary channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --chrome-sxs",     // User-level, secondary SxS canary mode.
        L"--uninstall --chrome-sxs",   // Update.
        install_static::CANARY_INDEX,  // Expect SxS canary mode.
        false,                         // Expect user-level.
        L"canary",                     // Expect canary channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    // System-level test cases.
    {
        L"setup.exe --system-level",   // System-level, primary mode.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        true,                          // Expect system-level.
        L"",                           // Expect stable channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --channel=beta --system-level",  // System-level, primary
                                                     // mode, beta channel.
        L"",                                         // New install.
        install_static::STABLE_INDEX,                // Expect primary mode.
        true,                                        // Expect system-level.
        L"beta",                                     // Expect beta channel.
        false,    // Expect not extended stable channel.
        L"beta",  // Expect the channel override.
    },
    {
        L"setup.exe --channel=beta --system-level",  // System-level, primary
                                                     // mode, beta channel.
        L"--uninstall --system-level",  // Updating an existing install.
        install_static::STABLE_INDEX,   // Expect primary mode.
        true,                           // Expect system-level.
        L"beta",                        // Expect beta channel.
        false,                          // Expect not extended stable channel.
        L"beta",                        // Expect the channel override.
    },
    {
        L"setup.exe --channel=dev --system-level",  // System-level, primary
                                                    // mode, dev channel.
        L"",                                        // New install.
        install_static::STABLE_INDEX,               // Expect primary mode.
        true,                                       // Expect system-level.
        L"dev",                                     // Expect dev channel.
        false,   // Expect not extended stable channel.
        L"dev",  // Expect the channel override.
    },
    {
        L"setup.exe --channel=dev --system-level",  // System-level, primary
                                                    // mode, dev channel.
        L"--uninstall --system-level",  // Updating an existing install.
        install_static::STABLE_INDEX,   // Expect primary mode.
        true,                           // Expect system-level.
        L"dev",                         // Expect dev channel.
        false,                          // Expect not extended stable channel.
        L"dev",                         // Expect the channel override.
    },
    {
        L"setup.exe --channel=bad --system-level",  // System-level, primary
                                                    // mode, bad channel.
        L"",                                        // New install.
        install_static::STABLE_INDEX,               // Expect primary mode.
        true,                                       // Expect system-level.
        L"",                                        // Expect stable channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --channel=bad --system-level",  // System-level, primary
                                                    // mode, bad channel.
        L"--uninstall --system-level",  // Updating an existing install.
        install_static::STABLE_INDEX,   // Expect primary mode.
        true,                           // Expect system-level.
        L"",                            // Expect stable channel.
        false,                          // Expect not extended stable channel.
        L"",                            // Expect no channel override.
    },
    {
        L"setup.exe --system-level",    // System-level, primary mode.
        L"--uninstall --system-level",  // Updating an existing install.
        install_static::STABLE_INDEX,   // Expect primary mode.
        true,                           // Expect system-level.
        L"",                            // Expect stable channel.
        false,                          // Expect not extended stable channel.
        L"",                            // Expect no channel override.
    },
    {
        L"setup.exe --system-level",   // System-level, primary mode.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        true,                          // Expect system-level.
        L"",                           // Expect stable channel.
        false,                         // Expect not extended stable channel.
        L"",                           // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-beta",  // User-level, secondary SxS
                                                    // beta mode.
        L"",                                        // New install.
        install_static::BETA_INDEX,                 // Expect SxS beta mode.
        true,                                       // Expect user-level.
        L"beta",                                    // Expect beta channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-beta",  // User-level, secondary SxS
                                                    // beta mode.
        L"--uninstall --system-level --chrome-beta",  // Update.
        install_static::BETA_INDEX,                   // Expect SxS beta mode.
        true,                                         // Expect user-level.
        L"beta",                                      // Expect beta channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-dev",  // User-level, secondary SxS
                                                   // dev mode.
        L"",                                       // New install.
        install_static::DEV_INDEX,                 // Expect SxS dev mode.
        true,                                      // Expect user-level.
        L"dev",                                    // Expect dev channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-dev",  // User-level, secondary SxS
                                                   // dev mode.
        L"--uninstall --system-level --chrome-dev",  // Update.
        install_static::DEV_INDEX,                   // Expect SxS dev mode.
        true,                                        // Expect user-level.
        L"dev",                                      // Expect dev channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-beta "
        L"--channel=dev",            // User-level, secondary SxS beta mode.
        L"",                         // New install.
        install_static::BETA_INDEX,  // Expect SxS beta mode.
        true,                        // Expect user-level.
        L"beta",                     // Expect beta channel.
        false,                       // Expect not extended stable channel.
        L"",                         // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-beta "
        L"--channel=dev",  // User-level secondary SxS beta mode.
        L"--uninstall --system-level --chrome-beta",  // Update.
        install_static::BETA_INDEX,                   // Expect SxS beta mode.
        true,                                         // Expect user-level.
        L"beta",                                      // Expect beta channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-dev "
        L"--channel=beta",          // User-level, secondary SxS dev mode.
        L"",                        // New install.
        install_static::DEV_INDEX,  // Expect SxS dev mode.
        true,                       // Expect user-level.
        L"dev",                     // Expect dev channel.
        false,                      // Expect not extended stable channel.
        L"",                        // Expect no channel override.
    },
    {
        L"setup.exe --system-level --chrome-dev "
        L"--channel=beta",  // User-level, secondary SxS dev mode.
        L"--uninstall --system-level --chrome-dev",  // Update.
        install_static::DEV_INDEX,                   // Expect SxS dev mode.
        true,                                        // Expect user-level.
        L"dev",                                      // Expect dev channel.
        false,  // Expect not extended stable channel.
        L"",    // Expect no channel override.
    },
    {
        L"setup.exe --system-level "
        L"--channel=extended",         // System-level, primary mode.
        L"",                           // New install.
        install_static::STABLE_INDEX,  // Expect primary mode.
        true,                          // Expect system-level.
        L"",                           // Expect stable channel.
        true,                          // Expect extended stable channel.
        L"extended",                   // Expect the channel override.
    },
};
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
constexpr TestData kTestData[] = {
    // User-level test cases.
    {
        L"setup.exe",  // User-level, primary mode.
        L"",           // New install.
        install_static::GOOGLE_CHROME_FOR_TESTING_INDEX,  // Expect primary
                                                          // mode.
        false,                                            // Expect user-level.
        L"",  // Expect empty channel.
    },
    {
        L"setup.exe",    // User-level, primary mode.
        L"--uninstall",  // Updating an existing install.
        install_static::GOOGLE_CHROME_FOR_TESTING_INDEX,  // Expect primary
                                                          // mode.
        false,                                            // Expect user-level.
        L"",  // Expect empty channel.
    },
};
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr TestData kTestData[] = {
    // User-level test cases.
    {
        L"setup.exe",                    // User-level, primary mode.
        L"",                             // New install.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        false,                           // Expect user-level.
        L"",                             // Expect empty channel.
    },
    {
        L"setup.exe",                    // User-level, primary mode.
        L"--uninstall",                  // Updating an existing install.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        false,                           // Expect user-level.
        L"",                             // Expect empty channel.
    },

    // System-level test cases.
    {
        L"setup.exe --system-level",     // System-level, primary mode.
        L"",                             // New install.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        true,                            // Expect system-level.
        L"",                             // Expect empty channel.
    },
    {
        L"setup.exe --system-level",     // System-level, primary mode.
        L"--uninstall --system-level",   // Updating an existing install.
        install_static::CHROMIUM_INDEX,  // Expect primary mode.
        true,                            // Expect system-level.
        L"",                             // Expect empty channel.
    },
};
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

class MakeInstallDetailsTest : public testing::TestWithParam<TestData> {
 public:
  MakeInstallDetailsTest(const MakeInstallDetailsTest&) = delete;
  MakeInstallDetailsTest& operator=(const MakeInstallDetailsTest&) = delete;

 protected:
  MakeInstallDetailsTest()
      : test_data_(GetParam()),
        root_key_(test_data_->system_level ? HKEY_LOCAL_MACHINE
                                           : HKEY_CURRENT_USER),
        nt_root_key_(test_data_->system_level ? nt::HKLM : nt::HKCU),
        command_line_(base::CommandLine::NO_PROGRAM) {
    // Prepare the inputs from the process command line.
    command_line_.ParseFromString(test_data_->command_line);
    initial_preferences_ =
        std::make_unique<installer::InitialPreferences>(command_line_);
  }

  void SetUp() override {
    std::wstring path;
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(root_key_, &path));
    nt::SetTestingOverride(nt_root_key_, path);

    // Prepare the inputs from the machine's state.
    ASSERT_NO_FATAL_FAILURE(SetUninstallArguments(
        root_key_, install_static::kInstallModes[test_data_->index].app_guid,
        test_data_->uninstall_args));
  }

  void TearDown() override {
    nt::SetTestingOverride(nt_root_key_, std::wstring());
  }

  const TestData& test_data() const { return *test_data_; }

  const base::CommandLine& command_line() const { return command_line_; }

  const installer::InitialPreferences& initial_preferences() const {
    return *initial_preferences_;
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

  registry_util::RegistryOverrideManager override_manager_;
  const raw_ref<const TestData> test_data_;
  HKEY root_key_;
  nt::ROOT_KEY nt_root_key_;
  base::CommandLine command_line_;
  std::unique_ptr<installer::InitialPreferences> initial_preferences_;
};

TEST_P(MakeInstallDetailsTest, Test) {
  std::unique_ptr<install_static::PrimaryInstallDetails> details(
      MakeInstallDetails(command_line(), initial_preferences()));
  EXPECT_THAT(details->install_mode_index(), Eq(test_data().index));
  EXPECT_THAT(details->system_level(), Eq(test_data().system_level));
  EXPECT_THAT(details->channel(), Eq(test_data().channel));
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_THAT(details->is_extended_stable_channel(),
              Eq(test_data().is_extended_stable_channel));
  EXPECT_THAT(details->channel_override(), Eq(test_data().channel_override));
#endif
}

INSTANTIATE_TEST_SUITE_P(All,
                         MakeInstallDetailsTest,
                         testing::ValuesIn(kTestData));
