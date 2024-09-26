// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/user_data_dir.h"

#include "base/ranges/algorithm.h"
#include "base/test/test_reg_util_win.h"
#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StrEq;

namespace install_static {
namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kPolicyRegistryKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";
const wchar_t kUserDataDirNameSuffix[] = L"\\Google\\Chrome\\User Data";
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
// kPolicyRegistryKey: same as Chromium
const wchar_t kPolicyRegistryKey[] = L"SOFTWARE\\Policies\\Chromium";
const wchar_t kUserDataDirNameSuffix[] =
    L"\\Google\\Chrome for Testing\\User Data";
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
    nt::SetTestingOverride(root_, std::wstring());
  }

 private:
  nt::ROOT_KEY root_;
};

TEST(UserDataDir, EmptyResultsInDefault) {
  std::wstring result, invalid;

  GetUserDataDirectoryImpl(L"m.exe", kFakeInstallConstants, &result, &invalid);
  EXPECT_TRUE(result.ends_with(kUserDataDirNameSuffix));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, InvalidResultsInDefault) {
  std::wstring result, invalid;

  GetUserDataDirectoryImpl(L"m.exe --user-data-dir=<>|:", kFakeInstallConstants,
                           &result, &invalid);
  EXPECT_TRUE(result.ends_with(kUserDataDirNameSuffix));
  EXPECT_EQ(L"<>|:", invalid);
}

TEST(UserDataDir, RegistrySettingsInHKLMOverrides) {
  std::wstring result, invalid;

  // Override the registry to say one value in HKLM, and confirm it takes
  // precedence over the command line in both implementations.
  registry_util::RegistryOverrideManager override_manager;
  std::wstring temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_LOCAL_MACHINE, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKLM, temp);

  base::win::RegKey key(HKEY_LOCAL_MACHINE, kPolicyRegistryKey, KEY_WRITE);
  LONG rv = key.WriteValue(kUserDataDirRegistryKey, L"yyy");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  GetUserDataDirectoryImpl(L"m.exe --user-data-dir=xxx", kFakeInstallConstants,
                           &result, &invalid);

  EXPECT_TRUE(result.ends_with(L"\\yyy"));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, RegistrySettingsInHKCUOverrides) {
  std::wstring result, invalid;

  // Override the registry to say one value in HKCU, and confirm it takes
  // precedence over the command line in both implementations.
  registry_util::RegistryOverrideManager override_manager;
  std::wstring temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_CURRENT_USER, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKCU, temp);

  base::win::RegKey key(HKEY_CURRENT_USER, kPolicyRegistryKey, KEY_WRITE);
  LONG rv = key.WriteValue(kUserDataDirRegistryKey, L"yyy");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  GetUserDataDirectoryImpl(L"m.exe --user-data-dir=xxx", kFakeInstallConstants,
                           &result, &invalid);

  EXPECT_TRUE(result.ends_with(L"\\yyy"));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, RegistrySettingsInHKLMTakesPrecedenceOverHKCU) {
  std::wstring result, invalid;

  // Override the registry in both HKLM and HKCU, and confirm HKLM takes
  // precedence.
  registry_util::RegistryOverrideManager override_manager;
  std::wstring temp;
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

  GetUserDataDirectoryImpl(L"m.exe --user-data-dir=xxx", kFakeInstallConstants,
                           &result, &invalid);

  EXPECT_TRUE(result.ends_with(L"\\111"));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, RegistrySettingWithPathExpansionHKCU) {
  std::wstring result, invalid;

  registry_util::RegistryOverrideManager override_manager;
  std::wstring temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_CURRENT_USER, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKCU, temp);
  base::win::RegKey key(HKEY_CURRENT_USER, kPolicyRegistryKey, KEY_WRITE);
  LONG rv = key.WriteValue(kUserDataDirRegistryKey, L"${windows}");
  ASSERT_EQ(rv, ERROR_SUCCESS);

  GetUserDataDirectoryImpl(L"m.exe --user-data-dir=xxx", kFakeInstallConstants,
                           &result, &invalid);

  EXPECT_EQ(strlen("X:\\WINDOWS"), result.size());
  EXPECT_EQ(std::wstring::npos, result.find(L"${windows}"));
  std::wstring upper;
  base::ranges::transform(result, std::back_inserter(upper), toupper);
  EXPECT_TRUE(upper.ends_with(L"\\WINDOWS"));
  EXPECT_EQ(std::wstring(), invalid);
}

TEST(UserDataDir, HasTempUserDataDirInHeadlessMode) {
  std::wstring result;
  std::wstring invalid;
  GetUserDataDirectoryImpl(L"m.exe --headless", kFakeInstallConstants, &result,
                           &invalid);
  EXPECT_THAT(result, HasSubstr(L"\\Headless"));
  EXPECT_THAT(invalid, IsEmpty());

  EXPECT_TRUE(IsTemporaryUserDataDirectoryCreatedForHeadless());

  EXPECT_TRUE(::RemoveDirectory(result.c_str()));
}

TEST(UserDataDir, HasNoTempUserDataDirInOldHeadlessMode) {
  std::wstring result;
  std::wstring invalid;
  GetUserDataDirectoryImpl(L"m.exe --headless=old", kFakeInstallConstants,
                           &result, &invalid);
  EXPECT_THAT(result, Not(HasSubstr(L"\\Headless")));
  EXPECT_THAT(invalid, IsEmpty());

  EXPECT_FALSE(IsTemporaryUserDataDirectoryCreatedForHeadless());
}

TEST(UserDataDir, HasNoHeadlessTempUserDataDirIfProvidedInCommandLine) {
  const std::wstring cmd_line_user_data_dir(L"C:\\UserDataDir");

  std::wstring result;
  std::wstring invalid;
  GetUserDataDirectoryImpl(
      L"m.exe --headless --user-data-dir=" + cmd_line_user_data_dir,
      kFakeInstallConstants, &result, &invalid);
  EXPECT_THAT(result, StrEq(cmd_line_user_data_dir));
  EXPECT_THAT(invalid, IsEmpty());

  EXPECT_FALSE(IsTemporaryUserDataDirectoryCreatedForHeadless());
}

TEST(UserDataDir, HasNoHeadlessTempUserDataDirIfProvidedByPolicy) {
  const std::wstring registry_user_data_dir(L"C:\\UserDataDir");

  registry_util::RegistryOverrideManager override_manager;
  std::wstring temp;
  ASSERT_NO_FATAL_FAILURE(
      override_manager.OverrideRegistry(HKEY_CURRENT_USER, &temp));
  ScopedNTRegistryTestingOverride nt_override(nt::HKCU, temp);

  base::win::RegKey key(HKEY_CURRENT_USER, kPolicyRegistryKey, KEY_WRITE);
  ASSERT_EQ(
      key.WriteValue(kUserDataDirRegistryKey, registry_user_data_dir.c_str()),
      ERROR_SUCCESS);

  std::wstring result;
  std::wstring invalid;
  GetUserDataDirectoryImpl(L"m.exe --headless", kFakeInstallConstants, &result,
                           &invalid);
  EXPECT_THAT(result, StrEq(registry_user_data_dir));
  EXPECT_THAT(invalid, IsEmpty());

  EXPECT_FALSE(IsTemporaryUserDataDirectoryCreatedForHeadless());
}

}  // namespace
}  // namespace install_static
