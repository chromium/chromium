// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <versionhelpers.h>  // windows.h must be before.

#include <tuple>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/chrome_elf/chrome_elf_constants.h"
#include "chrome/chrome_elf/chrome_elf_security.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool SetSecurityFinchFlag(bool creation) {
  bool success = true;
  const base::string16 finch_path(install_static::GetRegistryPath().append(
      elf_sec::kRegSecurityFinchKeyName));
  base::win::RegKey security_key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);

  if (creation) {
    if (ERROR_SUCCESS !=
        security_key.CreateKey(finch_path.c_str(), KEY_QUERY_VALUE))
      success = false;
  } else {
    if (ERROR_SUCCESS != security_key.DeleteKey(finch_path.c_str()))
      success = false;
  }

  security_key.Close();
  return success;
}

bool IsSecuritySet() {
  typedef decltype(GetProcessMitigationPolicy)* GetProcessMitigationPolicyFunc;

  // Check the settings from EarlyBrowserSecurity().
  if (::IsWindows8OrGreater()) {
    GetProcessMitigationPolicyFunc get_process_mitigation_policy =
        reinterpret_cast<GetProcessMitigationPolicyFunc>(::GetProcAddress(
            ::GetModuleHandleW(L"kernel32.dll"), "GetProcessMitigationPolicy"));
    if (!get_process_mitigation_policy)
      return false;

    // Check that extension points are disabled.
    // (Legacy hooking.)
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy = {};
    if (!get_process_mitigation_policy(::GetCurrentProcess(),
                                       ProcessExtensionPointDisablePolicy,
                                       &policy, sizeof(policy)))
      return false;

    return policy.DisableExtensionPoints;
  }

  return true;
}

void RegRedirect(nt::ROOT_KEY key,
                 registry_util::RegistryOverrideManager* rom) {
  ASSERT_NE(key, nt::AUTO);
  base::string16 temp;

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
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, base::string16()));
  else
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKLM, base::string16()));
}

TEST(ChromeElfUtilTest, BrowserProcessSecurityTest) {
  if (!::IsWindows8OrGreater())
    return;

  // Set up registry override for this test.
  registry_util::RegistryOverrideManager override_manager;
  ASSERT_NO_FATAL_FAILURE(RegRedirect(nt::HKCU, &override_manager));

  // First, ensure that the emergency-off finch signal works.
  EXPECT_TRUE(SetSecurityFinchFlag(true));
  elf_security::EarlyBrowserSecurity();
  EXPECT_FALSE(IsSecuritySet());
  EXPECT_TRUE(SetSecurityFinchFlag(false));

  // Second, test that the process mitigation is set when no finch signal.
  elf_security::EarlyBrowserSecurity();
  EXPECT_TRUE(IsSecuritySet());

  ASSERT_NO_FATAL_FAILURE(CancelRegRedirect(nt::HKCU));
}

}  // namespace
