// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/registry_util.h"

#include <stdint.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const wchar_t kRegistryKeyPath[] = L"Software\\foo";
const wchar_t kValueName[] = L"dummy";
const wchar_t kValue[] = L"This is a test";
const wchar_t kValueAsBinary[] =
    L"540068006900730020006900730020006100200074006500730074000000";
const BYTE kBytesValue[] = {1, 2, 3, 5, 8, 13};
const wchar_t kConvertedBytesValue[] = L"01020305080D";
const DWORD kDWORDValue = 42;
const wchar_t kDWORDRawValue[] = L"\x2a\x00\x00\x00";
const wchar_t kDWORDStringValue[] = L"0000002a";
const wchar_t kValueExpand[] = L"%PATH%";
const wchar_t kValueMulti[] = L"This\0is\0a\0multi\0value\0";
const wchar_t kValueWithNull[] = L"This\0is\0a\0test";
const wchar_t kValueEmpty[] = L"";
const wchar_t kUnicodeValue[] = L"This is the euro char: \u20AC";
const wchar_t kRegistryDefaultName[] = L"";
const BYTE kNoNullCharString[] = {65, 0, 66, 0};
const BYTE kSingleNullCharString[] = {65, 0, 66, 0, 0};
const wchar_t kExpectedSingleOrNoNullCharString[] = L"AB";
const BYTE kOddSizeEntryString[] = {0, 65, 0, 66, 0xFF, 0, 0, 1, 2};
const wchar_t kExpectedOddSizeEntryString[] = L"\x4100\x4200\xFF\x100\x2";
const BYTE kNullOddSizeEntryString[] = {0, 65, 0, 66, 0xFF, 0, 0, 0, 0};
const wchar_t kExpectedNullOddSizeEntryString[] = L"\x4100\x4200\xFF\0";
const wchar_t kRegistryKeyPathFoo[] = L"software\\foo";
const wchar_t kRegistryKeyPathFooBar[] = L"software\\foo\\bar";
const wchar_t kRegistryKeyPathFooBarBat[] = L"software\\foo\\bar\\bat";
const wchar_t kRegistryKeyPathFooBong[] = L"software\\foo\\bong";
const wchar_t kRegistryKeyPathFooBongBat[] = L"software\\foo\\bong\\bat";
const wchar_t kRegistryKeyPathClsidFoo[] =
    L"software\\classes\\clsid\\{deadbeef-0000-0000-0000-000000000000}";
const wchar_t kRegistryKeyPathClsidFoo32[] =
    L"software\\classes\\clsid\\{deadbeef-3200-0000-0000-000000000000}";
const wchar_t kRegistryKeyPathClsidFoo64[] =
    L"software\\classes\\clsid\\{deadbeef-6400-0000-0000-000000000000}";

void DeleteRegKeys(std::vector<base::win::RegKey>* keys) {
  for (auto& key : *keys)
    key.DeleteKey(L"");
}

}  // namespace

TEST(RegistryUtilTests, CollectMatchingRegistryNames) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey registry_key;

  ASSERT_EQ(ERROR_SUCCESS, registry_key.Create(HKEY_LOCAL_MACHINE, kValueName,
                                               KEY_ALL_ACCESS));

  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.WriteValue(kRegistryDefaultName, kValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(L"Test1", kValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(L"Test2", kValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(L"Test_3", kValue));

  std::vector<std::wstring> names;
  CollectMatchingRegistryNames(
      registry_key, L"Test*", PUPData::kRegistryPatternEscapeCharacter, &names);
  EXPECT_THAT(names, testing::ElementsAre(L"Test1", L"Test2", L"Test_3"));
  names.clear();

  CollectMatchingRegistryNames(
      registry_key, L"tEsT*", PUPData::kRegistryPatternEscapeCharacter, &names);
  EXPECT_THAT(names, testing::ElementsAre(L"Test1", L"Test2", L"Test_3"));
  names.clear();

  CollectMatchingRegistryNames(
      registry_key, L"t?s*", PUPData::kRegistryPatternEscapeCharacter, &names);
  EXPECT_THAT(names, testing::ElementsAre(L"Test1", L"Test2", L"Test_3"));
  names.clear();

  CollectMatchingRegistryNames(
      registry_key, L"test?", PUPData::kRegistryPatternEscapeCharacter, &names);
  EXPECT_THAT(names, testing::ElementsAre(L"Test1", L"Test2"));
  names.clear();

  CollectMatchingRegistryNames(registry_key, L"test??",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &names);
  EXPECT_THAT(names, testing::ElementsAre(L"Test_3"));
  names.clear();

  CollectMatchingRegistryNames(registry_key, LR"(test???)",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &names);
  EXPECT_TRUE(names.empty());

  CollectMatchingRegistryNames(
      registry_key, L"*", PUPData::kRegistryPatternEscapeCharacter, &names);
  EXPECT_THAT(names, testing::ElementsAre(kRegistryDefaultName, L"Test1",
                                          L"Test2", L"Test_3"));
  names.clear();
}

TEST(RegistryUtilTests, CollectMatchingRegistryNamesWithEscapedWildcard) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey registry_key;

  ASSERT_EQ(ERROR_SUCCESS, registry_key.Create(HKEY_LOCAL_MACHINE, kValueName,
                                               KEY_ALL_ACCESS));

  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.WriteValue(kRegistryDefaultName, kValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(L"*", kValue));
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.WriteValue(ESCAPE_REGISTRY_STR("*"), kValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(
                               L"\uFFFF" ESCAPE_REGISTRY_STR("*"), kValue));

  std::vector<std::wstring> names;
  CollectMatchingRegistryNames(registry_key, ESCAPE_REGISTRY_STR("*"),
                               PUPData::kRegistryPatternEscapeCharacter,
                               &names);
  EXPECT_THAT(names, testing::ElementsAre(L"*"));
}

TEST(RegistryUtilTests, CollectMatchingRegistryKeys) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey foo_registry_key;
  base::win::RegKey foo_bar_registry_key;
  base::win::RegKey foo_bar_bat_registry_key;
  base::win::RegKey foo_bong_registry_key;
  base::win::RegKey foo_bong_bat_registry_key;

  ASSERT_EQ(ERROR_SUCCESS,
            foo_registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryKeyPathFoo,
                                    KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS,
            foo_bar_registry_key.Create(
                HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBar, KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS,
            foo_bar_bat_registry_key.Create(
                HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBarBat, KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS,
            foo_bong_registry_key.Create(
                HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBong, KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS, foo_bong_bat_registry_key.Create(
                               HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBongBat,
                               KEY_ALL_ACCESS));

  std::vector<RegKeyPath> paths;
  CollectMatchingRegistryPaths(HKEY_LOCAL_MACHINE, L"software\\*",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &paths);
  EXPECT_THAT(paths,
              testing::ElementsAre(RegKeyPath(
                  HKEY_LOCAL_MACHINE, kRegistryKeyPathFoo, KEY_WOW64_32KEY)));
  paths.clear();

  CollectMatchingRegistryPaths(HKEY_LOCAL_MACHINE, L"software\\*\\*",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &paths);
  EXPECT_THAT(paths, testing::WhenSorted(testing::ElementsAre(
                         RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBar,
                                    KEY_WOW64_32KEY),
                         RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBong,
                                    KEY_WOW64_32KEY))));
  paths.clear();

  CollectMatchingRegistryPaths(HKEY_LOCAL_MACHINE, L"software\\f??\\*\\b*",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &paths);
  EXPECT_THAT(paths,
              testing::WhenSorted(testing::ElementsAre(
                  RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBarBat,
                             KEY_WOW64_32KEY),
                  RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathFooBongBat,
                             KEY_WOW64_32KEY))));
  paths.clear();

  CollectMatchingRegistryPaths(HKEY_LOCAL_MACHINE, L"software\\*o\\*o*\\*a*",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &paths);
  EXPECT_THAT(paths, testing::ElementsAre(RegKeyPath(HKEY_LOCAL_MACHINE,
                                                     kRegistryKeyPathFooBongBat,
                                                     KEY_WOW64_32KEY)));
  paths.clear();

  CollectMatchingRegistryPaths(HKEY_LOCAL_MACHINE, L"software\\//*",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &paths);
  EXPECT_TRUE(paths.empty());
}

TEST(RegistryUtilTests, CollectMatchingRegistryKeysWow) {
  // Ignore versions of Windows that don't perform registry redirection.
  if (!IsX64Architecture()) {
    LOG(WARNING) << "Skipping x64 specific test";
    return;
  }

  // DO NOT use the RegistryOverrideManager here. We need to access the real
  // deal to test key redirection.
  std::vector<base::win::RegKey> keys(4);
  base::ScopedClosureRunner cleanup(base::BindOnce(&DeleteRegKeys, &keys));
  ASSERT_EQ(ERROR_SUCCESS,
            keys[0].Create(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo,
                           KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  Sleep(1);  // Sleep for 1ms as a WAR for the registry's low timer resolution.
  ASSERT_EQ(ERROR_SUCCESS,
            keys[1].Create(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo,
                           KEY_ALL_ACCESS | KEY_WOW64_64KEY));
  ASSERT_EQ(ERROR_SUCCESS,
            keys[2].Create(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo32,
                           KEY_ALL_ACCESS | KEY_WOW64_32KEY));
  ASSERT_EQ(ERROR_SUCCESS,
            keys[3].Create(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo64,
                           KEY_ALL_ACCESS | KEY_WOW64_64KEY));

  std::vector<RegKeyPath> paths;
  CollectMatchingRegistryPaths(
      HKEY_LOCAL_MACHINE, L"software\\classes\\clsid\\{deadbeef-*}",
      PUPData::kRegistryPatternEscapeCharacter, &paths);

  // Note that we expect the same 'foo' key path twice because redirection means
  // it maps to a different underlying key.
  EXPECT_THAT(paths,
              testing::WhenSorted(testing::ElementsAre(
                  RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo,
                             KEY_WOW64_64KEY),
                  RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo64,
                             KEY_WOW64_64KEY),
                  RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo,
                             KEY_WOW64_32KEY),
                  RegKeyPath(HKEY_LOCAL_MACHINE, kRegistryKeyPathClsidFoo32,
                             KEY_WOW64_32KEY))));
}

TEST(RegistryUtilTests, CollectMatchingRegistryKeysWithEscapeCharacters) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey star_registry_key;
  base::win::RegKey dummy_registry_key;
  base::win::RegKey star_mark_registry_key;

  ASSERT_EQ(ERROR_SUCCESS,
            star_registry_key.Create(HKEY_LOCAL_MACHINE, L"software\\*",
                                     KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS,
            star_registry_key.Create(HKEY_LOCAL_MACHINE, L"software\\dummy",
                                     KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS,
            star_mark_registry_key.Create(HKEY_LOCAL_MACHINE, L"software\\*\\?",
                                          KEY_ALL_ACCESS));

  std::vector<RegKeyPath> paths;
  CollectMatchingRegistryPaths(HKEY_LOCAL_MACHINE, L"software\\*",
                               PUPData::kRegistryPatternEscapeCharacter,
                               &paths);
  EXPECT_THAT(
      paths,
      testing::WhenSorted(testing::ElementsAre(
          RegKeyPath(HKEY_LOCAL_MACHINE, L"software\\*", KEY_WOW64_32KEY),
          RegKeyPath(HKEY_LOCAL_MACHINE, L"software\\dummy",
                     KEY_WOW64_32KEY))));
  paths.clear();

  CollectMatchingRegistryPaths(
      HKEY_LOCAL_MACHINE, L"software\\" ESCAPE_REGISTRY_STR("*"),
      PUPData::kRegistryPatternEscapeCharacter, &paths);
  EXPECT_THAT(paths, testing::ElementsAre(RegKeyPath(
                         HKEY_LOCAL_MACHINE, L"software\\*", KEY_WOW64_32KEY)));
  paths.clear();

  CollectMatchingRegistryPaths(
      HKEY_LOCAL_MACHINE, L"software\\?\\" ESCAPE_REGISTRY_STR("?"),
      PUPData::kRegistryPatternEscapeCharacter, &paths);
  EXPECT_THAT(paths,
              testing::ElementsAre(RegKeyPath(
                  HKEY_LOCAL_MACHINE, L"software\\*\\?", KEY_WOW64_32KEY)));
  paths.clear();

  CollectMatchingRegistryPaths(
      HKEY_LOCAL_MACHINE, L"software\\" ESCAPE_REGISTRY_STR("?") L"\\?",
      PUPData::kRegistryPatternEscapeCharacter, &paths);
  EXPECT_TRUE(paths.empty());
}

TEST(RegistryUtilTests, OpenRegistryKey) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey registry_key;

  // Create a registry key to read back.
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                                KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName, kValue));

  // Try to read back the registry key with invalid access rights.
  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, kRegistryKeyPath);
  base::win::RegKey key;
  std::wstring content;
  uint32_t content_type;
  EXPECT_TRUE(key_path.Open(KEY_WRITE, &key));
  EXPECT_FALSE(
      ReadRegistryValue(key, kValueName, &content, &content_type, nullptr));

  // Read back the registry key with read access.
  EXPECT_TRUE(key_path.Open(KEY_READ, &key));
  EXPECT_TRUE(
      ReadRegistryValue(key, kValueName, &content, &content_type, nullptr));
  EXPECT_EQ(0, ::memcmp(content.c_str(), kValue,
                        std::max(sizeof(kValue), content.size())));
  EXPECT_EQ(REG_SZ, content_type);
}

TEST(RegistryUtilTests, ReadNonExistingRegistryValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  std::wstring content;
  uint32_t content_type = REG_NONE;

  EXPECT_FALSE(
      ReadRegistryValue(reg_key, kValueName, &content, &content_type, nullptr));
  EXPECT_TRUE(content.empty());
  EXPECT_EQ(REG_NONE, content_type);
}

TEST(RegistryUtilTests, ReadStringRegistryValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  std::wstring content;
  uint32_t content_type;

  // Write a string value and read it back.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kValue, sizeof(kValue), REG_SZ));
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, &content_type, nullptr));
  EXPECT_EQ(0, ::memcmp(content.c_str(), kValue,
                        std::max(sizeof(kValue), content.size())));
  EXPECT_EQ(REG_SZ, content_type);

  // Read registry must accept null content and content_type.
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, nullptr, nullptr, nullptr));
}

TEST(RegistryUtilTests, ReadExpandStringRegistryValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  std::wstring content;
  uint32_t content_type;

  // Write a string value and read it back.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kValueExpand, sizeof(kValueExpand),
                               REG_EXPAND_SZ));
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, &content_type, nullptr));
  EXPECT_EQ(0, ::memcmp(content.c_str(), kValueExpand,
                        std::max(sizeof(kValueExpand), content.size())));
  EXPECT_EQ(REG_EXPAND_SZ, content_type);
}

TEST(RegistryUtilTests, ReadMultiStringRegistryValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  std::wstring content;
  uint32_t content_type;

  // Write a multi-string value and read it back.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kValueMulti, sizeof(kValueMulti),
                               REG_MULTI_SZ));
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, &content_type, nullptr));
  EXPECT_EQ(0, ::memcmp(content.c_str(), kValueMulti,
                        std::max(sizeof(kValueMulti), content.size())));
  EXPECT_EQ(REG_MULTI_SZ, content_type);
}

TEST(RegistryUtilTests, ReadStringWithNullRegistryValue1) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  std::wstring content;

  // Write a string value with null characters and read it back.
  EXPECT_EQ(ERROR_SUCCESS, reg_key.WriteValue(kValueName, kValueWithNull,
                                              sizeof(kValueWithNull), REG_SZ));
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(0, ::memcmp(content.c_str(), kValueWithNull,
                        std::max(sizeof(kValueWithNull), content.size())));
}

TEST(RegistryUtilTests, ReadStringEmptyRegistryValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write an empty string (0 byte) value and read it back.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kValueEmpty, 0, REG_SZ));
  std::wstring content;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(0U, content.size());
}

TEST(RegistryUtilTests, ReadHugeStringRegistryValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  std::wstring content;

  // Write a huge string to the registry value and read it back.
  std::wstring huge_content(4096, 'x');
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, huge_content.c_str()));
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(content, huge_content);
}

TEST(RegistryUtilTests, ReadRegistryValueOfDWORDType) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write a DWORD value.
  EXPECT_EQ(ERROR_SUCCESS, reg_key.WriteValue(kValueName, kDWORDValue));

  std::wstring content;
  uint32_t content_type;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, &content_type, nullptr));
  EXPECT_EQ(kDWORDStringValue, content);
  EXPECT_EQ(REG_DWORD, content_type);
}

TEST(RegistryUtilTests, ReadRegistryValueOfBinaryType) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write a binary value.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kValue, sizeof(kValue), REG_BINARY));
  std::wstring content;
  uint32_t content_type;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, &content_type, nullptr));
  EXPECT_EQ(kValueAsBinary, content);
  EXPECT_EQ(REG_BINARY, content_type);
}

TEST(RegistryUtilTests, ReadRegistryValueOfSmallBinaryType) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write a binary value.
  EXPECT_EQ(ERROR_SUCCESS, reg_key.WriteValue(kValueName, "a", 1, REG_BINARY));
  std::wstring content;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(L"61", content);
}

TEST(RegistryUtilTests, ReadRegistryValueOfInvalidSize) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write a binary value.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kOddSizeEntryString,
                               sizeof(kOddSizeEntryString), REG_SZ));
  std::wstring content;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(kExpectedOddSizeEntryString, content);
}

TEST(RegistryUtilTests, ReadRegistryValueWithSingleNullTerminatingChar) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write a binary value.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kSingleNullCharString,
                               sizeof(kSingleNullCharString), REG_SZ));
  std::wstring content;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(kExpectedSingleOrNoNullCharString, content);
}

TEST(RegistryUtilTests, ReadRegistryValueWithNoNullTerminatingChar) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write a binary value.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kNoNullCharString,
                               sizeof(kNoNullCharString), REG_SZ));
  std::wstring content;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(kExpectedSingleOrNoNullCharString, content);
}

TEST(RegistryUtilTests, ReadRegistryValueWithExtraNullCharacter) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  base::win::RegKey reg_key(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_ALL_ACCESS);

  // Write a binary value with a extra-ending null character.
  EXPECT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(kValueName, kNullOddSizeEntryString,
                               sizeof(kNullOddSizeEntryString), REG_SZ));
  std::wstring content;
  EXPECT_TRUE(
      ReadRegistryValue(reg_key, kValueName, &content, nullptr, nullptr));
  EXPECT_EQ(0, ::memcmp(content.c_str(), kExpectedNullOddSizeEntryString,
                        std::max(sizeof(kExpectedNullOddSizeEntryString),
                                 content.size())));
}

TEST(RegistryUtilTests, GetRegistryValueAsStringRegularString) {
  std::wstring input_value(kUnicodeValue, std::size(kUnicodeValue) - 1);
  std::wstring output_value;

  GetRegistryValueAsString(input_value.c_str(),
                           input_value.size() * sizeof(wchar_t), REG_SZ,
                           &output_value);

  EXPECT_EQ(kUnicodeValue, output_value);
}

TEST(RegistryUtilTests, GetRegistryValueAsStringDword) {
  std::wstring input_value(kDWORDRawValue, sizeof(kDWORDValue));
  std::wstring output_value;

  GetRegistryValueAsString(input_value.c_str(),
                           input_value.size() * sizeof(wchar_t), REG_DWORD,
                           &output_value);

  EXPECT_EQ(kDWORDStringValue, output_value);
}

TEST(RegistryUtilTests, GetRegistryValueAsStringFakeBinary) {
  std::wstring output_value;

  GetRegistryValueAsString(kValue, sizeof(kValue), REG_BINARY, &output_value);

  EXPECT_EQ(kValueAsBinary, output_value);
}

TEST(RegistryUtilTests, GetRegistryValueAsStringRealBinary) {
  std::wstring output_value;

  GetRegistryValueAsString(reinterpret_cast<const wchar_t*>(kBytesValue),
                           std::size(kBytesValue), REG_BINARY, &output_value);

  EXPECT_EQ(kConvertedBytesValue, output_value);
}

TEST(RegistryUtilTests, GetRegistryValueAsStringSmallBinaries) {
  std::wstring output_value;
  GetRegistryValueAsString(reinterpret_cast<const wchar_t*>("a"), 1, REG_BINARY,
                           &output_value);
  EXPECT_EQ(L"61", output_value);

  GetRegistryValueAsString(L"a", 2, REG_BINARY, &output_value);
  EXPECT_EQ(L"6100", output_value);

  GetRegistryValueAsString(reinterpret_cast<const wchar_t*>("abc"), 3,
                           REG_BINARY, &output_value);
  EXPECT_EQ(L"616263", output_value);
}

}  // namespace chrome_cleaner
