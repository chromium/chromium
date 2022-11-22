// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

class ProductStateTest : public testing::TestWithParam<bool> {
 protected:
  ProductStateTest();

  void SetUp() override;

  void ApplyUninstallCommand(const wchar_t* exe_path, const wchar_t* args);
  void MinimallyInstallProduct(const wchar_t* version);

  const bool system_install_;
  const HKEY overridden_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  base::win::RegKey clients_;
  base::win::RegKey client_state_;
};

ProductStateTest::ProductStateTest()
    : system_install_(GetParam()),
      overridden_(system_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER) {}

void ProductStateTest::SetUp() {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_manager_.OverrideRegistry(overridden_));

  ASSERT_EQ(
      ERROR_SUCCESS,
      clients_.Create(overridden_, install_static::GetClientsKeyPath().c_str(),
                      KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  ASSERT_EQ(ERROR_SUCCESS,
            client_state_.Create(
                overridden_, install_static::GetClientStateKeyPath().c_str(),
                KEY_ALL_ACCESS | KEY_WOW64_32KEY));
}

void ProductStateTest::MinimallyInstallProduct(const wchar_t* version) {
  EXPECT_EQ(ERROR_SUCCESS,
            clients_.WriteValue(google_update::kRegVersionField, version));
}

void ProductStateTest::ApplyUninstallCommand(const wchar_t* exe_path,
                                             const wchar_t* args) {
  if (exe_path == nullptr) {
    LONG result = client_state_.DeleteValue(kUninstallStringField);
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
  } else {
    EXPECT_EQ(ERROR_SUCCESS,
              client_state_.WriteValue(kUninstallStringField, exe_path));
  }

  if (args == nullptr) {
    LONG result = client_state_.DeleteValue(kUninstallArgumentsField);
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
  } else {
    EXPECT_EQ(ERROR_SUCCESS,
              client_state_.WriteValue(kUninstallArgumentsField, args));
  }
}

TEST_P(ProductStateTest, InitializeInstalled) {
  // Not installed.
  {
    ProductState state;
    LONG result = clients_.DeleteValue(google_update::kRegVersionField);
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_FALSE(state.Initialize(system_install_));
  }

  // Empty version.
  {
    ProductState state;
    LONG result = clients_.WriteValue(google_update::kRegVersionField, L"");
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_FALSE(state.Initialize(system_install_));
  }

  // Bogus version.
  {
    ProductState state;
    LONG result =
        clients_.WriteValue(google_update::kRegVersionField, L"goofy");
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_FALSE(state.Initialize(system_install_));
  }

  // Valid "pv" value.
  {
    ProductState state;
    LONG result =
        clients_.WriteValue(google_update::kRegVersionField, L"10.0.47.0");
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_EQ("10.0.47.0", state.version().GetString());
  }
}

// Test extraction of the "opv" value from the Clients key.
TEST_P(ProductStateTest, InitializeOldVersion) {
  MinimallyInstallProduct(L"10.0.1.1");

  // No "opv" value.
  {
    ProductState state;
    LONG result = clients_.DeleteValue(google_update::kRegOldVersionField);
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_EQ(state.old_version(), nullptr);
  }

  // Empty "opv" value.
  {
    ProductState state;
    LONG result = clients_.WriteValue(google_update::kRegOldVersionField, L"");
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_EQ(state.old_version(), nullptr);
  }

  // Bogus "opv" value.
  {
    ProductState state;
    LONG result =
        clients_.WriteValue(google_update::kRegOldVersionField, L"coming home");
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_EQ(state.old_version(), nullptr);
  }

  // Valid "opv" value.
  {
    ProductState state;
    LONG result =
        clients_.WriteValue(google_update::kRegOldVersionField, L"10.0.47.0");
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_NE(state.old_version(), nullptr);
    EXPECT_EQ("10.0.47.0", state.old_version()->GetString());
  }
}

// Test extraction of the uninstall command and arguments from the ClientState
// key.
TEST_P(ProductStateTest, InitializeUninstallCommand) {
  MinimallyInstallProduct(L"10.0.1.1");

  // No uninstall command.
  {
    ProductState state;
    ApplyUninstallCommand(nullptr, nullptr);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_TRUE(state.GetSetupPath().empty());
    EXPECT_TRUE(state.uninstall_command().GetCommandLineString().empty());
    EXPECT_TRUE(state.uninstall_command().GetSwitches().empty());
  }

  // Empty values.
  {
    ProductState state;
    ApplyUninstallCommand(L"", L"");
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_TRUE(state.GetSetupPath().empty());
    EXPECT_TRUE(state.uninstall_command().GetCommandLineString().empty());
    EXPECT_TRUE(state.uninstall_command().GetSwitches().empty());
  }

  // Uninstall command without exe.
  {
    ProductState state;
    ApplyUninstallCommand(nullptr, L"--uninstall");
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_TRUE(state.GetSetupPath().empty());
    EXPECT_EQ(L" --uninstall",
              state.uninstall_command().GetCommandLineString());
    EXPECT_EQ(1U, state.uninstall_command().GetSwitches().size());
  }

  // Uninstall command without args.
  {
    ProductState state;
    ApplyUninstallCommand(L"setup.exe", nullptr);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_EQ(L"setup.exe", state.GetSetupPath().value());
    EXPECT_EQ(L"setup.exe", state.uninstall_command().GetCommandLineString());
    EXPECT_TRUE(state.uninstall_command().GetSwitches().empty());
  }

  // Uninstall command with exe that requires quoting.
  {
    ProductState state;
    ApplyUninstallCommand(L"set up.exe", nullptr);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_EQ(L"set up.exe", state.GetSetupPath().value());
    EXPECT_EQ(L"\"set up.exe\"",
              state.uninstall_command().GetCommandLineString());
    EXPECT_TRUE(state.uninstall_command().GetSwitches().empty());
  }

  // Uninstall command with both exe and args.
  {
    ProductState state;
    ApplyUninstallCommand(L"setup.exe", L"--uninstall");
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_EQ(L"setup.exe", state.GetSetupPath().value());
    EXPECT_EQ(L"setup.exe --uninstall",
              state.uninstall_command().GetCommandLineString());
    EXPECT_EQ(1U, state.uninstall_command().GetSwitches().size());
  }
}

// Test extraction of the msi marker from the ClientState key.
TEST_P(ProductStateTest, InitializeMsi) {
  MinimallyInstallProduct(L"10.0.1.1");

  // No msi marker.
  {
    ProductState state;
    LONG result = client_state_.DeleteValue(google_update::kRegMSIField);
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_FALSE(state.is_msi());
  }

  // Msi marker set to zero.
  {
    ProductState state;
    EXPECT_EQ(ERROR_SUCCESS,
              client_state_.WriteValue(google_update::kRegMSIField,
                                       static_cast<DWORD>(0)));
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_FALSE(state.is_msi());
  }

  // Msi marker set to one.
  {
    ProductState state;
    EXPECT_EQ(ERROR_SUCCESS,
              client_state_.WriteValue(google_update::kRegMSIField,
                                       static_cast<DWORD>(1)));
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_TRUE(state.is_msi());
  }

  // Msi marker set to a bogus DWORD.
  {
    ProductState state;
    EXPECT_EQ(ERROR_SUCCESS,
              client_state_.WriteValue(google_update::kRegMSIField,
                                       static_cast<DWORD>(47)));
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_TRUE(state.is_msi());
  }

  // Msi marker set to a bogus string.
  {
    ProductState state;
    EXPECT_EQ(ERROR_SUCCESS,
              client_state_.WriteValue(google_update::kRegMSIField, L"bogus!"));
    EXPECT_TRUE(state.Initialize(system_install_));
    EXPECT_FALSE(state.is_msi());
  }
}

INSTANTIATE_TEST_SUITE_P(UserLevel, ProductStateTest, ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SystemLevel,
                         ProductStateTest,
                         ::testing::Values(true));

}  // namespace installer
