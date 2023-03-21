// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/uninstallation_via_os_settings.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr wchar_t kUninstallRegistryKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";

class RegisterUninstallationViaOsSettingsTest : public testing::Test {
 protected:
  void SetUp() override {
    const HKEY kRoot = HKEY_CURRENT_USER;
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(kRoot));
    ASSERT_TRUE(
        base::win::RegKey(kRoot, kUninstallRegistryKey, KEY_CREATE_SUB_KEY)
            .Valid());

    ASSERT_EQ(uninstall_key_.Create(kRoot, kUninstallRegistryKey, KEY_WRITE),
              ERROR_SUCCESS);

    uninstall_commandline_ =
        std::make_unique<base::CommandLine>(base::FilePath(L"chrome.exe"));
    uninstall_commandline_->AppendSwitchNative(switches::kProfileDirectory,
                                               L"Default");
    uninstall_commandline_->AppendSwitchNative(switches::kUninstallAppId,
                                               L"fhkpfpfifagoflnfpbdgegnghg");
  }

  base::win::ScopedCOMInitializer com_initializer_;
  base::win::RegKey uninstall_reg_key_;

  const std::wstring register_key_ = L"12cde1ab";
  std::unique_ptr<base::CommandLine> uninstall_commandline_;
  base::win::RegKey uninstall_key_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

}  // namespace

// Test that the uninstall registry key are inserted during shortcut
// creation on the Start Menu only.
TEST_F(RegisterUninstallationViaOsSettingsTest, DuplicateKey) {
  ASSERT_TRUE(RegisterUninstallationViaOsSettings(
      register_key_, L"Display_Name", L"Chromium", *uninstall_commandline_,
      base::FilePath(L"C:\\users\\account\\AppData\\Local\\Chromium\\User "
                     "Data\\Default\\Icons\\icon.ico")));

  ASSERT_EQ(uninstall_key_.OpenKey(register_key_.c_str(), KEY_QUERY_VALUE),
            ERROR_SUCCESS);

  // It should be failed for duplicate key.
  ASSERT_FALSE(RegisterUninstallationViaOsSettings(
      register_key_, L"Display_Name", L"Chromium", *uninstall_commandline_,
      base::FilePath(L"C:\\users\\account\\AppData\\Local\\Chromium\\User "
                     "Data\\Default\\Icons\\icon.ico")));

  UnregisterUninstallationViaOsSettings(register_key_);

  ASSERT_FALSE(
      base::win::RegKey(
          HKEY_CURRENT_USER,
          (std::wstring(kUninstallRegistryKey) + register_key_).c_str(),
          KEY_QUERY_VALUE)
          .Valid());
}

TEST_F(RegisterUninstallationViaOsSettingsTest, RegValues) {
  // Registry entry is inserted as it has --app-id argument and
  // uninstall_string property set.
  std::wstring display_name = L"Display_Name";
  std::wstring publisher = L"Chromium";

  base::FilePath icon_path(
      L"C:\\users\\account\\AppData\\Local\\Chromium\\User "
      "Data\\Default\\Icons\\icon.ico");

  ASSERT_TRUE(RegisterUninstallationViaOsSettings(
      register_key_, display_name, publisher, *uninstall_commandline_,
      icon_path));

  ASSERT_EQ(uninstall_key_.OpenKey(register_key_.c_str(), KEY_QUERY_VALUE),
            ERROR_SUCCESS);

  std::wstring value;
  ASSERT_EQ(uninstall_key_.ReadValue(L"DisplayName", &value), ERROR_SUCCESS);
  EXPECT_EQ(value, display_name);

  ASSERT_EQ(uninstall_key_.ReadValue(L"DisplayVersion", &value), ERROR_SUCCESS);
  EXPECT_EQ(value, L"1.0");

  ASSERT_EQ(uninstall_key_.ReadValue(L"ApplicationVersion", &value),
            ERROR_SUCCESS);
  EXPECT_EQ(value, L"1.0");

  ASSERT_EQ(uninstall_key_.ReadValue(L"InstallDate", &value), ERROR_SUCCESS);

  ASSERT_EQ(uninstall_key_.ReadValue(L"UninstallString", &value),
            ERROR_SUCCESS);
  EXPECT_EQ(value,
            L"chrome.exe --profile-directory=Default "
            "--uninstall-app-id=fhkpfpfifagoflnfpbdgegnghg");

  DWORD int_value;
  ASSERT_EQ(uninstall_key_.ReadValueDW(L"NoRepair", &int_value), ERROR_SUCCESS);
  EXPECT_EQ(int_value, 1U);

  ASSERT_EQ(uninstall_key_.ReadValueDW(L"NoModify", &int_value), ERROR_SUCCESS);
  EXPECT_EQ(int_value, 1U);

  ASSERT_EQ(uninstall_key_.ReadValue(L"DisplayIcon", &value), ERROR_SUCCESS);
  EXPECT_EQ(value, icon_path.value());

  EXPECT_TRUE(UnregisterUninstallationViaOsSettings(register_key_));
}
