// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_state.h"

#include <windows.h>

#include <stddef.h>

#include <fstream>
#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/fake_installation_state.h"
#include "chrome/installer/util/fake_product_state.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using installer::InitialPreferences;
using installer::InstallationState;
using installer::InstallerState;
using registry_util::RegistryOverrideManager;

class InstallerStateTest : public testing::Test {
 public:
  InstallerStateTest(const InstallerStateTest&) = delete;
  InstallerStateTest& operator=(const InstallerStateTest&) = delete;

 protected:
  InstallerStateTest() {}

  void SetUp() override { ASSERT_TRUE(test_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir test_dir_;
};

// An installer state on which we can access otherwise protected members.
class MockInstallerState : public InstallerState {
 public:
  MockInstallerState() : InstallerState() {}
  void set_target_path(const base::FilePath& target_path) {
    target_path_ = target_path;
  }
  const base::Version& critical_update_version() const {
    return critical_update_version_;
  }
};

TEST_F(InstallerStateTest, WithProduct) {
  const bool system_level = true;
  base::CommandLine cmd_line = base::CommandLine::FromString(
      std::wstring(L"setup.exe") + (system_level ? L" --system-level" : L""));
  InitialPreferences prefs(cmd_line);
  InstallationState machine_state;
  machine_state.Initialize();
  MockInstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, machine_state);
  installer_state.set_target_path(test_dir_.GetPath());
  EXPECT_EQ(system_level, installer_state.system_install());

  const char kCurrentVersion[] = "1.2.3.4";
  base::Version current_version(kCurrentVersion);

  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  EXPECT_EQ(root, installer_state.root_key());

  {
    RegistryOverrideManager override_manager;
    ASSERT_NO_FATAL_FAILURE(override_manager.OverrideRegistry(root));
    RegKey chrome_key(root, install_static::GetClientsKeyPath().c_str(),
                      KEY_ALL_ACCESS);
    EXPECT_TRUE(chrome_key.Valid());
    if (chrome_key.Valid()) {
      chrome_key.WriteValue(
          google_update::kRegVersionField,
          base::UTF8ToWide(current_version.GetString()).c_str());
      machine_state.Initialize();
      // TODO(tommi): Also test for when there exists a new_chrome.exe.
      base::Version found_version(
          installer_state.GetCurrentVersion(machine_state));
      EXPECT_TRUE(found_version.IsValid());
      if (found_version.IsValid())
        EXPECT_EQ(current_version, found_version);
    }
  }
}

TEST_F(InstallerStateTest, InstallerResult) {
  const bool system_level = true;
  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring launch_cmd = L"hey diddle diddle";
  std::wstring value;
  DWORD dw_value;

  // Check results for a fresh install of Chrome.
  constexpr wchar_t command_line[] = L"setup.exe --system-level";

  RegistryOverrideManager override_manager;
  ASSERT_NO_FATAL_FAILURE(override_manager.OverrideRegistry(root));
  base::CommandLine cmd_line = base::CommandLine::FromString(command_line);
  const InitialPreferences prefs(cmd_line);
  InstallationState machine_state;
  machine_state.Initialize();
  InstallerState state;
  state.Initialize(cmd_line, prefs, machine_state);
  state.WriteInstallerResult(installer::FIRST_INSTALL_SUCCESS,
                             IDS_INSTALL_OS_ERROR_BASE, &launch_cmd);
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(root, install_static::GetClientStateKeyPath().c_str(),
                     KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValueDW(installer::kInstallerResult, &dw_value));
  EXPECT_EQ(static_cast<DWORD>(0), dw_value);
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValueDW(installer::kInstallerError, &dw_value));
  EXPECT_EQ(static_cast<DWORD>(installer::FIRST_INSTALL_SUCCESS), dw_value);
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValue(installer::kInstallerResultUIString, &value));
  EXPECT_FALSE(value.empty());
  EXPECT_EQ(ERROR_SUCCESS,
            key.ReadValue(installer::kInstallerSuccessLaunchCmdLine, &value));
  EXPECT_EQ(launch_cmd, value);
}

TEST_F(InstallerStateTest, InitializeTwice) {
  // Override these paths so that they can be found after the registry override
  // manager is in place.
  base::FilePath temp;
  base::PathService::Get(base::DIR_PROGRAM_FILES, &temp);
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES,
                                                  temp);
  base::PathService::Get(base::DIR_PROGRAM_FILESX86, &temp);
  base::ScopedPathOverride program_filesx86_override(base::DIR_PROGRAM_FILESX86,
                                                     temp);
  base::PathService::Get(base::DIR_LOCAL_APP_DATA, &temp);
  base::ScopedPathOverride local_app_data_override(base::DIR_LOCAL_APP_DATA,
                                                   temp);
  registry_util::RegistryOverrideManager override_manager;
  ASSERT_NO_FATAL_FAILURE(override_manager.OverrideRegistry(HKEY_CURRENT_USER));
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE));

  InstallationState machine_state;
  machine_state.Initialize();

  InstallerState installer_state;

  // Initialize the instance to install user-level Chrome.
  {
    base::CommandLine cmd_line(base::CommandLine::FromString(L"setup.exe"));
    InitialPreferences prefs(cmd_line);
    installer_state.Initialize(cmd_line, prefs, machine_state);
  }
  // Confirm the expected state.
  EXPECT_EQ(InstallerState::USER_LEVEL, installer_state.level());
  EXPECT_EQ(InstallerState::SINGLE_INSTALL_OR_UPDATE,
            installer_state.operation());
  EXPECT_TRUE(wcsstr(installer_state.target_path().value().c_str(),
                     install_static::GetChromeInstallSubDirectory().c_str()));
  EXPECT_FALSE(installer_state.verbose_logging());
  EXPECT_EQ(installer_state.state_key(),
            install_static::GetClientStateKeyPath());

  // Now initialize it to install system-level Chrome.
  {
    base::CommandLine cmd_line(base::CommandLine::FromString(
        L"setup.exe --system-level --verbose-logging"));
    InitialPreferences prefs(cmd_line);
    installer_state.Initialize(cmd_line, prefs, machine_state);
  }

  // Confirm that the old state is gone.
  EXPECT_EQ(InstallerState::SYSTEM_LEVEL, installer_state.level());
  EXPECT_EQ(InstallerState::SINGLE_INSTALL_OR_UPDATE,
            installer_state.operation());
  EXPECT_TRUE(wcsstr(installer_state.target_path().value().c_str(),
                     install_static::GetChromeInstallSubDirectory().c_str()));
  EXPECT_TRUE(installer_state.verbose_logging());
  EXPECT_EQ(installer_state.state_key(),
            install_static::GetClientStateKeyPath());
}

// A fixture for testing InstallerState::DetermineCriticalVersion.  Individual
// tests must invoke Initialize() with a critical version.
class InstallerStateCriticalVersionTest : public ::testing::Test {
 protected:
  InstallerStateCriticalVersionTest()
      : low_version_("15.0.874.106"),
        opv_version_("15.0.874.255"),
        middle_version_("16.0.912.32"),
        pv_version_("16.0.912.255"),
        high_version_("17.0.932.0"),
        cmd_line_(base::CommandLine::NO_PROGRAM) {}

  // Initializes the InstallerState to use for a test run.  The returned
  // instance's critical update version is set to |version|.  |version| may be
  // nullptr, in which case the critical update version is unset.
  MockInstallerState& Initialize(const base::Version& version) {
    cmd_line_ = !version.IsValid()
                    ? base::CommandLine::FromString(L"setup.exe")
                    : base::CommandLine::FromString(
                          L"setup.exe --critical-update-version=" +
                          base::ASCIIToWide(version.GetString()));
    prefs_ = std::make_unique<InitialPreferences>(cmd_line_);
    machine_state_.Initialize();
    installer_state_.Initialize(cmd_line_, *prefs_, machine_state_);
    return installer_state_;
  }

  const base::Version low_version_;
  const base::Version opv_version_;
  const base::Version middle_version_;
  const base::Version pv_version_;
  const base::Version high_version_;

  base::CommandLine cmd_line_;
  std::unique_ptr<InitialPreferences> prefs_;
  InstallationState machine_state_;
  MockInstallerState installer_state_;
};

// Test the case where the critical version is less than the currently-running
// Chrome.  The critical version is ignored since it doesn't apply.
TEST_F(InstallerStateCriticalVersionTest, CriticalBeforeOpv) {
  MockInstallerState& installer_state(Initialize(low_version_));

  EXPECT_EQ(installer_state.critical_update_version(), low_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(base::Version(), pv_version_)
          .IsValid());
  // Installed version is past the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(opv_version_, pv_version_)
          .IsValid());
  // Installed version is past the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, pv_version_)
          .IsValid());
}

// Test the case where the critical version is equal to the currently-running
// Chrome.  The critical version is ignored since it doesn't apply.
TEST_F(InstallerStateCriticalVersionTest, CriticalEqualsOpv) {
  MockInstallerState& installer_state(Initialize(opv_version_));

  EXPECT_EQ(installer_state.critical_update_version(), opv_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(base::Version(), pv_version_)
          .IsValid());
  // Installed version equals the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(opv_version_, pv_version_)
          .IsValid());
  // Installed version equals the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, pv_version_)
          .IsValid());
}

// Test the case where the critical version is between the currently-running
// Chrome and the to-be-installed Chrome.
TEST_F(InstallerStateCriticalVersionTest, CriticalBetweenOpvAndPv) {
  MockInstallerState& installer_state(Initialize(middle_version_));

  EXPECT_EQ(installer_state.critical_update_version(), middle_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(base::Version(), pv_version_)
          .IsValid());
  // Installed version before the critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(opv_version_, pv_version_)
          .IsValid());
  // Installed version is past the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, pv_version_)
          .IsValid());
}

// Test the case where the critical version is the same as the to-be-installed
// Chrome.
TEST_F(InstallerStateCriticalVersionTest, CriticalEqualsPv) {
  MockInstallerState& installer_state(Initialize(pv_version_));

  EXPECT_EQ(installer_state.critical_update_version(), pv_version_);
  // Unable to determine the installed version, so assume critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(base::Version(), pv_version_)
          .IsValid());
  // Installed version before the critical update.
  EXPECT_TRUE(
      installer_state.DetermineCriticalVersion(opv_version_, pv_version_)
          .IsValid());
  // Installed version equals the critical update.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, pv_version_)
          .IsValid());
}

// Test the case where the critical version is greater than the to-be-installed
// Chrome.
TEST_F(InstallerStateCriticalVersionTest, CriticalAfterPv) {
  MockInstallerState& installer_state(Initialize(high_version_));

  EXPECT_EQ(installer_state.critical_update_version(), high_version_);
  // Critical update newer than the new version.
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(base::Version(), pv_version_)
          .IsValid());
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(opv_version_, pv_version_)
          .IsValid());
  EXPECT_FALSE(
      installer_state.DetermineCriticalVersion(pv_version_, pv_version_)
          .IsValid());
}
