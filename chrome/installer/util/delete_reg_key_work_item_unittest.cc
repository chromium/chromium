// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/delete_reg_key_work_item.h"

#include <windows.h>

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/win/registry.h"
#include "base/win/security_descriptor.h"
#include "chrome/installer/util/registry_test_data.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

class DeleteRegKeyWorkItemTest : public testing::Test {
 protected:
  static void TearDownTestCase() { logging::CloseLogFile(); }

  void SetUp() override {
    ASSERT_TRUE(test_data_.Initialize(HKEY_CURRENT_USER, L"SOFTWARE\\TmpTmp"));
  }

  RegistryTestData test_data_;
};

// Test that deleting a key that doesn't exist succeeds, and that rollback does
// nothing.
TEST_F(DeleteRegKeyWorkItemTest, TestNoKey) {
  const std::wstring key_paths[] = {
      std::wstring(test_data_.base_path() + L"\\NoKeyHere"),
      std::wstring(test_data_.base_path() + L"\\NoKeyHere\\OrHere")};
  RegKey key;
  for (size_t i = 0; i < std::size(key_paths); ++i) {
    const std::wstring& key_path = key_paths[i];
    std::unique_ptr<DeleteRegKeyWorkItem> item(
        WorkItem::CreateDeleteRegKeyWorkItem(test_data_.root_key(), key_path,
                                             WorkItem::kWow64Default));
    EXPECT_TRUE(item->Do());
    EXPECT_NE(ERROR_SUCCESS,
              key.Open(test_data_.root_key(), key_path.c_str(), KEY_READ));
    item->Rollback();
    item.reset();
    EXPECT_NE(ERROR_SUCCESS,
              key.Open(test_data_.root_key(), key_path.c_str(), KEY_READ));
  }
}

// Test that deleting an empty key succeeds, and that rollback brings it back.
TEST_F(DeleteRegKeyWorkItemTest, TestEmptyKey) {
  RegKey key;
  const std::wstring& key_path = test_data_.empty_key_path();
  std::unique_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(test_data_.root_key(), key_path,
                                           WorkItem::kWow64Default));
  EXPECT_TRUE(item->Do());
  EXPECT_NE(ERROR_SUCCESS,
            key.Open(test_data_.root_key(), key_path.c_str(), KEY_READ));
  item->Rollback();
  item.reset();
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(test_data_.root_key(), key_path.c_str(), KEY_READ));
}

// Test that deleting a key with subkeys and values succeeds, and that rollback
// brings them all back.
TEST_F(DeleteRegKeyWorkItemTest, TestNonEmptyKey) {
  RegKey key;
  const std::wstring& key_path = test_data_.non_empty_key_path();
  std::unique_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(test_data_.root_key(), key_path,
                                           WorkItem::kWow64Default));
  EXPECT_TRUE(item->Do());
  EXPECT_NE(ERROR_SUCCESS,
            key.Open(test_data_.root_key(), key_path.c_str(), KEY_READ));
  item->Rollback();
  item.reset();
  test_data_.ExpectMatchesNonEmptyKey(test_data_.root_key(), key_path.c_str());
}

// Test that deleting a key with subkeys we can't delete fails, and that
// everything is there after rollback.
// Showing as flaky on windows.
// http://crbug.com/74654
TEST_F(DeleteRegKeyWorkItemTest, DISABLED_TestUndeletableKey) {
  RegKey key;
  std::wstring key_name(test_data_.base_path() + L"\\UndeletableKey");
  EXPECT_EQ(ERROR_SUCCESS,
            key.Create(test_data_.root_key(), key_name.c_str(), KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(nullptr, key_name.c_str()));
  DWORD dw_value = 1;
  RegKey subkey;
  RegKey subkey2;
  EXPECT_EQ(ERROR_SUCCESS,
            subkey.Create(key.Handle(), L"Subkey", KEY_WRITE | WRITE_DAC));
  EXPECT_EQ(ERROR_SUCCESS, subkey.WriteValue(L"SomeValue", 1U));
  EXPECT_EQ(ERROR_SUCCESS,
            subkey2.Create(subkey.Handle(), L"Subkey2", KEY_WRITE | WRITE_DAC));
  EXPECT_EQ(ERROR_SUCCESS, subkey2.WriteValue(L"", 2U));
  // builtin users read.
  auto sd = base::win::SecurityDescriptor::FromSddl(L"D:PAI(A;OICI;KR;;;BU)");
  ASSERT_TRUE(sd.has_value());
  SECURITY_DESCRIPTOR sec_desc;
  sd->ToAbsolute(sec_desc);
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegSetKeySecurity(subkey.Handle(), DACL_SECURITY_INFORMATION, &sec_desc));
  // builtin users all access.
  sd = base::win::SecurityDescriptor::FromSddl(L"D:PAI(A;OICI;KA;;;BU)");
  ASSERT_TRUE(sd.has_value());
  sd->ToAbsolute(sec_desc);
  EXPECT_EQ(ERROR_SUCCESS,
            RegSetKeySecurity(subkey2.Handle(), DACL_SECURITY_INFORMATION,
                              &sec_desc));
  subkey2.Close();
  subkey.Close();
  key.Close();
  std::unique_ptr<DeleteRegKeyWorkItem> item(
      WorkItem::CreateDeleteRegKeyWorkItem(test_data_.root_key(), key_name,
                                           WorkItem::kWow64Default));
  EXPECT_FALSE(item->Do());
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(test_data_.root_key(), key_name.c_str(), KEY_QUERY_VALUE));
  item->Rollback();
  item.reset();
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(test_data_.root_key(), key_name.c_str(), KEY_QUERY_VALUE));
  std::wstring str_value;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(nullptr, &str_value));
  EXPECT_EQ(key_name, str_value);
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(L"Subkey", KEY_READ | WRITE_DAC));
  dw_value = 0;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"SomeValue", &dw_value));
  EXPECT_EQ(1U, dw_value);
  // Give users all access to the subkey so it can be deleted.
  EXPECT_EQ(
      ERROR_SUCCESS,
      RegSetKeySecurity(key.Handle(), DACL_SECURITY_INFORMATION, &sec_desc));
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(L"Subkey2", KEY_QUERY_VALUE));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(L"", &dw_value));
  EXPECT_EQ(2U, dw_value);
}
