// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <tuple>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/chrome_elf/chrome_elf_security.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool SetExtensionPointEnabledFlag(bool creation) {
  bool success = true;
  const std::wstring reg_path(install_static::GetRegistryPath().append(
      elf_sec::kRegBrowserExtensionPointKeyName));
  base::win::RegKey security_key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);

  if (creation) {
    if (ERROR_SUCCESS !=
        security_key.CreateKey(reg_path.c_str(), KEY_QUERY_VALUE))
      success = false;
  } else {
    if (ERROR_SUCCESS != security_key.DeleteKey(reg_path.c_str()))
      success = false;
  }

  security_key.Close();
  return success;
}

bool IsSecuritySet() {
  // Check that extension points are disabled. (Legacy hooking.)
  PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
  if (!::GetProcessMitigationPolicy(::GetCurrentProcess(),
                                    ProcessExtensionPointDisablePolicy, &policy,
                                    sizeof(policy))) {
    return false;
  }
  return policy.DisableExtensionPoints;
}

void RegRedirect(nt::ROOT_KEY key,
                 registry_util::RegistryOverrideManager* rom) {
  ASSERT_NE(key, nt::AUTO);
  std::wstring temp;

  if (key == nt::HKCU) {
    ASSERT_NO_FATAL_FAILURE(rom->OverrideRegistry(HKEY_CURRENT_USER, &temp));
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, temp));
  } else {
    ASSERT_NO_FATAL_FAILURE(rom->OverrideRegistry(HKEY_LOCAL_MACHINE, &temp));
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKLM, temp));
  }
}

void CancelRegRedirect(nt::ROOT_KEY key) {
  ASSERT_NE(key, nt::AUTO);
  if (key == nt::HKCU)
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, std::wstring()));
  else
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKLM, std::wstring()));
}

TEST(ChromeElfUtilTest, ValidateExtensionPointCallComesFromDLL) {
  // We should validate the exe version isn't used for this test
  elf_security::ValidateExeForTesting(true);

  // This is the setting from the elf dll load in the test
  EXPECT_EQ(::IsExtensionPointDisableSet(), IsSecuritySet());
}

TEST(ChromeElfUtilTest, BrowserProcessSecurityTest) {
  // Set up registry override for this test.
  registry_util::RegistryOverrideManager override_manager;
  ASSERT_NO_FATAL_FAILURE(RegRedirect(nt::HKCU, &override_manager));

  // We need to turn off validating the exe for this test
  elf_security::ValidateExeForTesting(false);

  // First, ensure that the policy is not applied without the reg key.
  elf_security::EarlyBrowserSecurity();
  EXPECT_FALSE(IsSecuritySet());
  EXPECT_FALSE(elf_security::IsExtensionPointDisableSet());
  EXPECT_TRUE(SetExtensionPointEnabledFlag(true));

  // Second, test that the process mitigation is set when the reg key exists.
  elf_security::EarlyBrowserSecurity();
  EXPECT_TRUE(IsSecuritySet());
  EXPECT_TRUE(elf_security::IsExtensionPointDisableSet());

  ASSERT_NO_FATAL_FAILURE(CancelRegRedirect(nt::HKCU));
}

}  // namespace
