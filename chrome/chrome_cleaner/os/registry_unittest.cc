// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/registry.h"

#include <windows.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

// Construct paths from a randomly-generated GUID, which is unlikely to
// conflict with anything else.
const wchar_t kRegistryKeyPathEnvTest[] =
    L"environment\\chrome-cleanup-tool-test-4ee21cdb-ecc5-47d6-aadc-"
    L"342ee9dcb463";
const wchar_t kRegistryKeyPathEnvTestUpCase[] =
    L"ENVIRONMENT\\CHROME-CLEANUP-TOOL-TEST-4EE21CDB-ECC5-47D6-AADC-"
    L"342EE9DCB463";

const wchar_t kRegistryKeyPathClsidTest[] =
    L"software\\classes\\clsid\\{4ee21cdb-ecc5-47d6-aadc-342ee9dcb463}";

const wchar_t kHardwareKeyPath[] = L"hardware";
const wchar_t kNativeHardwareKeyPath[] = L"\\registry\\machine\\hardware";
const wchar_t kSoftwareKeyPath[] = L"software";
const wchar_t kSoftwareKeyPathUpCase[] = L"SOFTWARE";
const wchar_t kNativeSoftwareKeyPath[] = L"\\registry\\machine\\software";
const wchar_t kNativeSoftwareKeyPath32[] =
    L"\\registry\\machine\\software\\wow6432node";

void DeleteRegKeys(std::vector<base::win::RegKey>* keys) {
  for (auto& key : *keys)
    key.DeleteKey(L"");
}

}  // namespace

TEST(RegistryTests, EquivalenceRedirected) {
  // Ignore versions of Windows that don't perform registry redirection.
  if (!IsX64Architecture()) {
    LOG(WARNING) << "Skipping x64 specific test";
    return;
  }

  // DO NOT use the RegistryOverrideManager here. We need to access the real
  // deal to test key redirection.
  const RegKeyPath path32(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidTest,
                          KEY_WOW64_32KEY);
  const RegKeyPath path64(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidTest,
                          KEY_WOW64_64KEY);
  const RegKeyPath default_path(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidTest);
  std::vector<base::win::RegKey> keys(2);
  base::ScopedClosureRunner cleanup(base::BindOnce(&DeleteRegKeys, &keys));
  ASSERT_TRUE(path32.Create(KEY_ALL_ACCESS, &keys[0]));
  ASSERT_TRUE(path64.Create(KEY_ALL_ACCESS, &keys[1]));

  // The 32 and 64 bit-specific paths are neither identical nor equivalent.
  EXPECT_FALSE(path32 == path64);
  EXPECT_FALSE(path32.IsEquivalent(path64));

  // But they are equivalent to the default path depending on the bitness.
  if (IsX64Process()) {
    EXPECT_TRUE(path64.IsEquivalent(default_path));
  } else {
    EXPECT_TRUE(path32.IsEquivalent(default_path));
  }
}

TEST(RegistryTests, EquivalenceNonRedirected) {
  // DO NOT use the RegistryOverrideManager here. We need to access the real
  // deal to test key redirection.
  const RegKeyPath path32(HKEY_CURRENT_USER, kRegistryKeyPathEnvTest,
                          KEY_WOW64_32KEY);
  const RegKeyPath path64(HKEY_CURRENT_USER, kRegistryKeyPathEnvTest,
                          KEY_WOW64_64KEY);
  std::vector<base::win::RegKey> keys(1);
  base::ScopedClosureRunner cleanup(base::BindOnce(&DeleteRegKeys, &keys));
  ASSERT_TRUE(path32.Create(KEY_ALL_ACCESS, &keys[0]));

  // The paths are not identical, but equivalent.
  EXPECT_FALSE(path32 == path64);
  EXPECT_TRUE(path32.IsEquivalent(path64));
}

TEST(RegistryTests, EquivalenceNonRedirectedUpCase) {
  // DO NOT use the RegistryOverrideManager here. We need to access the real
  // deal to test key redirection.
  const RegKeyPath path32(HKEY_CURRENT_USER, kRegistryKeyPathEnvTestUpCase,
                          KEY_WOW64_32KEY);
  const RegKeyPath path64(HKEY_CURRENT_USER, kRegistryKeyPathEnvTest,
                          KEY_WOW64_64KEY);
  std::vector<base::win::RegKey> keys(1);
  base::ScopedClosureRunner cleanup(base::BindOnce(&DeleteRegKeys, &keys));
  ASSERT_TRUE(path32.Create(KEY_ALL_ACCESS, &keys[0]));

  // The paths are not identical, but equivalent.
  EXPECT_FALSE(path32 == path64);
  EXPECT_TRUE(path32.IsEquivalent(path64));
}

TEST(RegistryTests, ComparatorIgnoreCase) {
  const RegKeyPath path1(HKEY_CURRENT_USER, kRegistryKeyPathEnvTest,
                         KEY_WOW64_32KEY);
  const RegKeyPath path2(HKEY_CURRENT_USER, kRegistryKeyPathEnvTestUpCase,
                         KEY_WOW64_32KEY);
  const RegKeyPath path3(HKEY_CURRENT_USER, kRegistryKeyPathClsidTest,
                         KEY_WOW64_32KEY);

  EXPECT_TRUE(path1 == path2);
  EXPECT_FALSE(path1 == path3);

  EXPECT_FALSE(path1 < path2);
  EXPECT_FALSE(path2 < path1);

  EXPECT_TRUE(path1 < path3);
  EXPECT_FALSE(path3 < path1);
}

TEST(RegistryTests, FullPath) {
  EXPECT_EQ(base::StrCat({L"HKLM\\", kRegistryKeyPathEnvTest}),
            RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathEnvTest).FullPath());
  EXPECT_EQ(
      base::StrCat({L"HKLM:32\\", kRegistryKeyPathEnvTest}),
      RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathEnvTest, KEY_WOW64_32KEY)
          .FullPath());
  EXPECT_EQ(
      base::StrCat({L"HKLM:64\\", kRegistryKeyPathEnvTest}),
      RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathEnvTest, KEY_WOW64_64KEY)
          .FullPath());

  EXPECT_EQ(L"HKLM\\X", RegKeyPath(HKEY_LOCAL_MACHINE, L"X").FullPath());
}

TEST(RegistryTests, GetNativeFullPath) {
  // The 'HKLM\\hardware' registry key doesn't have redirection.
  std::wstring native_path;
  EXPECT_TRUE(RegKeyPath(HKEY_LOCAL_MACHINE, kHardwareKeyPath)
                  .GetNativeFullPath(&native_path));
  EXPECT_TRUE(
      WStringEqualsCaseInsensitive(native_path, kNativeHardwareKeyPath));
  EXPECT_TRUE(RegKeyPath(HKEY_LOCAL_MACHINE, kHardwareKeyPath, KEY_WOW64_32KEY)
                  .GetNativeFullPath(&native_path));
  EXPECT_TRUE(
      WStringEqualsCaseInsensitive(native_path, kNativeHardwareKeyPath));
  EXPECT_TRUE(RegKeyPath(HKEY_LOCAL_MACHINE, kHardwareKeyPath, KEY_WOW64_64KEY)
                  .GetNativeFullPath(&native_path));
  EXPECT_TRUE(
      WStringEqualsCaseInsensitive(native_path, kNativeHardwareKeyPath));

  // The 'HKLM\\software' registry key may have a redirection.
  std::wstring native_path32;
  EXPECT_TRUE(RegKeyPath(HKEY_LOCAL_MACHINE, kSoftwareKeyPath, KEY_WOW64_32KEY)
                  .GetNativeFullPath(&native_path32));
  EXPECT_TRUE(
      WStringEqualsCaseInsensitive(native_path32, kNativeSoftwareKeyPath) ||
      WStringEqualsCaseInsensitive(native_path32, kNativeSoftwareKeyPath32));

  std::wstring native_path64;
  EXPECT_TRUE(RegKeyPath(HKEY_LOCAL_MACHINE, kSoftwareKeyPath, KEY_WOW64_64KEY)
                  .GetNativeFullPath(&native_path64));
  EXPECT_TRUE(
      WStringEqualsCaseInsensitive(native_path64, kNativeSoftwareKeyPath));
}

TEST(RegistryTests, GetNativeFullPathUpCase) {
  std::wstring native_path32;
  EXPECT_TRUE(
      RegKeyPath(HKEY_LOCAL_MACHINE, kSoftwareKeyPathUpCase, KEY_WOW64_32KEY)
          .GetNativeFullPath(&native_path32));
  EXPECT_TRUE(
      WStringEqualsCaseInsensitive(native_path32, kNativeSoftwareKeyPath) ||
      WStringEqualsCaseInsensitive(native_path32, kNativeSoftwareKeyPath32));
}

}  // namespace chrome_cleaner
