// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_reg_value_work_item.h"

#include <windows.h>

#include <memory>
#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

const wchar_t kTestKey[] = L"DeleteRegValueWorkItemTest";
const wchar_t kNameStr[] = L"name_str";
const wchar_t kNameDword[] = L"name_dword";

class DeleteRegValueWorkItemTest : public testing::Test {
 public:
  DeleteRegValueWorkItemTest(const DeleteRegValueWorkItemTest&) = delete;
  DeleteRegValueWorkItemTest& operator=(const DeleteRegValueWorkItemTest&) =
      delete;

 protected:
  DeleteRegValueWorkItemTest() {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

}  // namespace

// Delete a value. The value should get deleted after Do() and should be
// recreated after Rollback().
TEST_F(DeleteRegValueWorkItemTest, DeleteExistingValue) {
  RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_CURRENT_USER, kTestKey, KEY_READ | KEY_WRITE));
  const std::wstring data_str(L"data_111");
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kNameStr, data_str.c_str()));
  const DWORD data_dword = 100;
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kNameDword, data_dword));

  static const wchar_t kNameEmpty[](L"name_empty");
  ASSERT_EQ(ERROR_SUCCESS,
            RegSetValueEx(key.Handle(), kNameEmpty, 0, REG_SZ, nullptr, 0));

  std::unique_ptr<DeleteRegValueWorkItem> work_item1(
      WorkItem::CreateDeleteRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameStr));
  std::unique_ptr<DeleteRegValueWorkItem> work_item2(
      WorkItem::CreateDeleteRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameDword));
  std::unique_ptr<DeleteRegValueWorkItem> work_item3(
      WorkItem::CreateDeleteRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameEmpty));

  EXPECT_TRUE(key.HasValue(kNameStr));
  EXPECT_TRUE(key.HasValue(kNameDword));
  EXPECT_TRUE(key.HasValue(kNameEmpty));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());
  EXPECT_TRUE(work_item3->Do());

  EXPECT_FALSE(key.HasValue(kNameStr));
  EXPECT_FALSE(key.HasValue(kNameDword));
  EXPECT_FALSE(key.HasValue(kNameEmpty));

  work_item1->Rollback();
  work_item2->Rollback();
  work_item3->Rollback();

  EXPECT_TRUE(key.HasValue(kNameStr));
  EXPECT_TRUE(key.HasValue(kNameDword));
  EXPECT_TRUE(key.HasValue(kNameEmpty));

  std::wstring read_str;
  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(kNameStr, &read_str));
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValueDW(kNameDword, &read_dword));
  EXPECT_EQ(read_str, data_str);
  EXPECT_EQ(read_dword, data_dword);

  // Verify empty value.
  DWORD type = 0;
  DWORD size = 0;
  EXPECT_EQ(ERROR_SUCCESS, key.ReadValue(kNameEmpty, nullptr, &size, &type));
  EXPECT_EQ(static_cast<DWORD>(REG_SZ), type);
  EXPECT_EQ(0u, size);
}

// Try deleting a value that doesn't exist.
TEST_F(DeleteRegValueWorkItemTest, DeleteNonExistentValue) {
  RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_CURRENT_USER, kTestKey, KEY_READ | KEY_WRITE));
  EXPECT_FALSE(key.HasValue(kNameStr));
  EXPECT_FALSE(key.HasValue(kNameDword));

  std::unique_ptr<DeleteRegValueWorkItem> work_item1(
      WorkItem::CreateDeleteRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameStr));
  std::unique_ptr<DeleteRegValueWorkItem> work_item2(
      WorkItem::CreateDeleteRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameDword));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  EXPECT_FALSE(key.HasValue(kNameStr));
  EXPECT_FALSE(key.HasValue(kNameDword));

  work_item1->Rollback();
  work_item2->Rollback();

  EXPECT_FALSE(key.HasValue(kNameStr));
  EXPECT_FALSE(key.HasValue(kNameDword));
}

// Try deleting a value whose key doesn't even exist.
TEST_F(DeleteRegValueWorkItemTest, DeleteValueInNonExistentKey) {
  RegKey key;
  // Confirm the key doesn't exist.
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ));

  std::unique_ptr<DeleteRegValueWorkItem> work_item1(
      WorkItem::CreateDeleteRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameStr));
  std::unique_ptr<DeleteRegValueWorkItem> work_item2(
      WorkItem::CreateDeleteRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameDword));

  EXPECT_TRUE(work_item1->Do());
  EXPECT_TRUE(work_item2->Do());

  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ));

  work_item1->Rollback();
  work_item2->Rollback();

  EXPECT_EQ(ERROR_FILE_NOT_FOUND,
            key.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ));
}
