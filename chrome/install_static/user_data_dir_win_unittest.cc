// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/test/test_reg_util_win.h"
#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/user_data_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace install_static {
namespace {

inline bool EndsWith(const std::wstring& value, const std::wstring& ending) {
  if (ending.size() > value.size())
    return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kPolicyRegistryKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";
const wchar_t kUserDataDirNameSuffix[] = L"\\Google\\Chrome\\User Data";
#else
const wchar_t kPolicyRegistryKey[] = L"SOFTWARE\\Policies\\Chromium";
const wchar_t kUserDataDirNameSuffix[] = L"\\Chromium\\User Data";
#endif

const wchar_t kUserDataDirRegistryKey[] = L"UserDataDir";

const InstallConstants kFakeInstallConstants = {
    sizeof(InstallConstants), 0, "", L"", L"", L"", L""};

class ScopedNTRegistryTestingOverride {
 public:
  ScopedNTRegistryTestingOverride(nt::ROOT_KEY root, const std::wstring& path)
      : root_(root) {
    EXPECT_TRUE(nt::SetTestingOverride(root_, path));
  }
  ~ScopedNTRegistryTestingOverride() {
    nt::SetTestingOverride(root_, base::string16());
  }

 private:
  nt::ROOT_KEY root_;
};

TEST(UserDataDir, EmptyResultsInDefault) {
  std::wstring result, invalid;

  install_static::GetUserDataDirectoryImpl(L"", kFakeInstallConstants, &result,
                                           &invalid);
  EXPECT_TRUE(EndsWith(result, kUserDataDirNameSuffix));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, InvalidResultsInDefault) {
  std::wstring result, invalid;

  install_static::GetUserDataDirectoryImpl(L"<>|:", kFakeInstallConstants,
                                           &result, &invalid);
  EXPECT_TRUE(EndsWith(result, kUserDataDirNameSuffix));
  EXPECT_EQ(L"<>|:", invalid);
}

TEST(UserDataDir, RegistrySettingsInHKLMOverrides) {
  std::wstring result, invalid;

  // Override the registry to say one value in HKLM, and confirm it takes
  // precedence over the command line in both implementations.
  registry_util::RegistryOverrideManager override_manager;
  base::string16 temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKLM, temp);

  base::win::RegKey key(HKEY_LOCAL_MACHINE, kPolicyRegistryKey, KEY_WRITE);
  LONG rv = key.WriteValue(kUserDataDirRegistryKey, L"yyy");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  install_static::GetUserDataDirectoryImpl(L"xxx", kFakeInstallConstants,
                                           &result, &invalid);

  EXPECT_TRUE(EndsWith(result, L"\\yyy"));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, RegistrySettingsInHKCUOverrides) {
  std::wstring result, invalid;

  // Override the registry to say one value in HKCU, and confirm it takes
  // precedence over the command line in both implementations.
  registry_util::RegistryOverrideManager override_manager;
  base::string16 temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_CURRENT_USER, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKCU, temp);

  base::win::RegKey key(HKEY_CURRENT_USER, kPolicyRegistryKey, KEY_WRITE);
  LONG rv = key.WriteValue(kUserDataDirRegistryKey, L"yyy");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  install_static::GetUserDataDirectoryImpl(L"xxx", kFakeInstallConstants,
                                           &result, &invalid);

  EXPECT_TRUE(EndsWith(result, L"\\yyy"));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, RegistrySettingsInHKLMTakesPrecedenceOverHKCU) {
  std::wstring result, invalid;

  // Override the registry in both HKLM and HKCU, and confirm HKLM takes
  // precedence.
  registry_util::RegistryOverrideManager override_manager;
  base::string16 temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKLM, temp);
  LONG rv;
  base::win::RegKey key1(HKEY_LOCAL_MACHINE, kPolicyRegistryKey, KEY_WRITE);
  rv = key1.WriteValue(kUserDataDirRegistryKey, L"111");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_CURRENT_USER, &temp));
  ScopedNTRegistryTestingOverride nt_override2(nt::HKCU, temp);
  base::win::RegKey key2(HKEY_CURRENT_USER, kPolicyRegistryKey, KEY_WRITE);
  rv = key2.WriteValue(kUserDataDirRegistryKey, L"222");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  install_static::GetUserDataDirectoryImpl(L"xxx", kFakeInstallConstants,
                                           &result, &invalid);

  EXPECT_TRUE(EndsWith(result, L"\\111"));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, RegistrySettingWithPathExpansionHKCU) {
  std::wstring result, invalid;

  registry_util::RegistryOverrideManager override_manager;
  base::string16 temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_CURRENT_USER, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKCU, temp);
  base::win::RegKey key(HKEY_CURRENT_USER, kPolicyRegistryKey, KEY_WRITE);
  LONG rv = key.WriteValue(kUserDataDirRegistryKey, L"${windows}");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  install_static::GetUserDataDirectoryImpl(L"xxx", kFakeInstallConstants,
                                           &result, &invalid);

  EXPECT_EQ(strlen("X:\\WINDOWS"), result.size());
  EXPECT_EQ(std::wstring::npos, result.find(L"${windows}"));
  std::wstring upper;
  std::transform(result.begin(), result.end(), std::back_inserter(upper),
                 toupper);
  EXPECT_TRUE(EndsWith(upper, L"\\WINDOWS"));
  EXPECT_EQ(std::wstring(), invalid);
}

}  // namespace
}  // namespace install_static
