// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/scoped_user_protocol_entry.h"

#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/registry_entry.h"
#include "chrome/installer/util/shell_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class ScopedUserProtocolEntryTest : public testing::Test {
 protected:
  static const wchar_t kProtocolEntryKeyPath[];
  static const wchar_t kProtocolEntrySubKeyPath[];
  static const wchar_t kProtocolEntryName[];
  static const wchar_t kProtocolEntryFakeName[];
  static const wchar_t kProtocolEntryFakeValue[];

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_overrides_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_FALSE(
        RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName, std::wstring())
            .KeyExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
  }

  void CreateNewRegistryValue(const std::wstring& key_path,
                              const std::wstring& name,
                              const std::wstring& value) {
    std::vector<std::unique_ptr<RegistryEntry>> entries;
    entries.push_back(std::make_unique<RegistryEntry>(key_path, name, value));
    ASSERT_TRUE(ShellUtil::AddRegistryEntries(HKEY_CURRENT_USER, entries));
  }

  void CreateScopedUserProtocolEntryAndVerifyRegistryValue(
      const std::wstring& expected_entry_value) {
    entry_ = std::make_unique<ScopedUserProtocolEntry>(L"http");
    ASSERT_TRUE(RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName,
                              expected_entry_value)
                    .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
  }

  registry_util::RegistryOverrideManager registry_overrides_manager_;
  std::unique_ptr<ScopedUserProtocolEntry> entry_;
};

const wchar_t ScopedUserProtocolEntryTest::kProtocolEntryKeyPath[] =
    L"Software\\Classes\\http";
const wchar_t ScopedUserProtocolEntryTest::kProtocolEntrySubKeyPath[] =
    L"Software\\Classes\\http\\sub";
const wchar_t ScopedUserProtocolEntryTest::kProtocolEntryName[] =
    L"URL Protocol";
const wchar_t ScopedUserProtocolEntryTest::kProtocolEntryFakeName[] =
    L"Fake URL Protocol";
const wchar_t ScopedUserProtocolEntryTest::kProtocolEntryFakeValue[] =
    L"Fake Value";

TEST_F(ScopedUserProtocolEntryTest, CreateKeyWhenMissingTest) {
  CreateScopedUserProtocolEntryAndVerifyRegistryValue(std::wstring());
  entry_.reset();
  ASSERT_FALSE(
      RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName, std::wstring())
          .KeyExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
}

TEST_F(ScopedUserProtocolEntryTest, DontTouchExistedKeyTest) {
  CreateNewRegistryValue(kProtocolEntryKeyPath, kProtocolEntryName,
                         kProtocolEntryFakeValue);
  ASSERT_TRUE(RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName,
                            kProtocolEntryFakeValue)
                  .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
  CreateScopedUserProtocolEntryAndVerifyRegistryValue(kProtocolEntryFakeValue);
  entry_.reset();
  ASSERT_TRUE(RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName,
                            kProtocolEntryFakeValue)
                  .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
}

TEST_F(ScopedUserProtocolEntryTest, EntryValueIsChangedTest) {
  CreateScopedUserProtocolEntryAndVerifyRegistryValue(std::wstring());
  CreateNewRegistryValue(kProtocolEntryKeyPath, kProtocolEntryName,
                         kProtocolEntryFakeValue);
  entry_.reset();
  ASSERT_TRUE(RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName,
                            kProtocolEntryFakeValue)
                  .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
}

TEST_F(ScopedUserProtocolEntryTest, AnotherEntryIsCreatedTest) {
  CreateScopedUserProtocolEntryAndVerifyRegistryValue(std::wstring());
  CreateNewRegistryValue(kProtocolEntryKeyPath, kProtocolEntryFakeName,
                         kProtocolEntryFakeValue);
  entry_.reset();
  ASSERT_TRUE(RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryFakeName,
                            kProtocolEntryFakeValue)
                  .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
  ASSERT_TRUE(
      RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName, std::wstring())
          .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
}

TEST_F(ScopedUserProtocolEntryTest, SubKeyIsCreatedTest) {
  CreateScopedUserProtocolEntryAndVerifyRegistryValue(std::wstring());
  CreateNewRegistryValue(kProtocolEntrySubKeyPath, kProtocolEntryName,
                         std::wstring());
  entry_.reset();
  ASSERT_TRUE(RegistryEntry(kProtocolEntrySubKeyPath, kProtocolEntryName,
                            std::wstring())
                  .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
  ASSERT_TRUE(
      RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName, std::wstring())
          .ExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
}

TEST_F(ScopedUserProtocolEntryTest, KeyHasBeenDeletedByOthersTest) {
  CreateScopedUserProtocolEntryAndVerifyRegistryValue(std::wstring());
  base::win::RegKey key(HKEY_CURRENT_USER, L"", KEY_WRITE);
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteKey(kProtocolEntryKeyPath));
  entry_.reset();
  ASSERT_FALSE(
      RegistryEntry(kProtocolEntryKeyPath, kProtocolEntryName, std::wstring())
          .KeyExistsInRegistry(RegistryEntry::LOOK_IN_HKCU));
}
