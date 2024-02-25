// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/create_reg_key_work_item.h"

#include <windows.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace {

class CreateRegKeyWorkItemTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_TRUE(RegKey(HKEY_CURRENT_USER, test_root, KEY_SET_VALUE).Valid());
  }
  void TearDown() override {
    logging::CloseLogFile();
  }

  static constexpr wchar_t test_root[] = L"TmpTmp";

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

}  // namespace

TEST_F(CreateRegKeyWorkItemTest, CreateKey) {
  RegKey key;

  base::FilePath parent_key(test_root);
  parent_key = parent_key.AppendASCII("a");
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER,
                                      parent_key.value().c_str(), KEY_READ));

  base::FilePath top_key_to_create(parent_key);
  top_key_to_create = top_key_to_create.AppendASCII("b");

  base::FilePath key_to_create(top_key_to_create);
  key_to_create = key_to_create.AppendASCII("c");
  key_to_create = key_to_create.AppendASCII("d");

  std::unique_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create.value(), WorkItem::kWow64Default));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create.value().c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should delete all the keys up to top_key_to_create.
  EXPECT_NE(
      ERROR_SUCCESS,
      key.Open(HKEY_CURRENT_USER, top_key_to_create.value().c_str(), KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, parent_key.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, CreateExistingKey) {
  RegKey key;

  base::FilePath key_to_create(test_root);
  key_to_create = key_to_create.AppendASCII("aa");
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER,
                                      key_to_create.value().c_str(), KEY_READ));

  std::unique_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create.value(), WorkItem::kWow64Default));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create.value().c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should not remove the key since it exists before
  // the CreateRegKeyWorkItem is called.
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, CreateSharedKey) {
  RegKey key;
  base::FilePath key_to_create_1(test_root);
  key_to_create_1 = key_to_create_1.AppendASCII("aaa");

  base::FilePath key_to_create_2(key_to_create_1);
  key_to_create_2 = key_to_create_2.AppendASCII("bbb");

  base::FilePath key_to_create_3(key_to_create_2);
  key_to_create_3 = key_to_create_3.AppendASCII("ccc");

  std::unique_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create_3.value(), WorkItem::kWow64Default));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create_3.value().c_str(), KEY_READ));

  // Create another key under key_to_create_2
  base::FilePath key_to_create_4(key_to_create_2);
  key_to_create_4 = key_to_create_4.AppendASCII("ddd");
  ASSERT_EQ(
      ERROR_SUCCESS,
      key.Create(HKEY_CURRENT_USER, key_to_create_4.value().c_str(), KEY_READ));

  work_item->Rollback();

  // Rollback should delete key_to_create_3.
  EXPECT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create_3.value().c_str(), KEY_READ));

  // Rollback should not delete key_to_create_2 as it is shared.
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create_2.value().c_str(), KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create_4.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, RollbackWithMissingKey) {
  RegKey key;
  base::FilePath key_to_create_1(test_root);
  key_to_create_1 = key_to_create_1.AppendASCII("aaaa");

  base::FilePath key_to_create_2(key_to_create_1);
  key_to_create_2 = key_to_create_2.AppendASCII("bbbb");

  base::FilePath key_to_create_3(key_to_create_2);
  key_to_create_3 = key_to_create_3.AppendASCII("cccc");

  std::unique_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create_3.value(), WorkItem::kWow64Default));

  EXPECT_TRUE(work_item->Do());

  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create_3.value().c_str(), KEY_READ));
  key.Close();

  // now delete key_to_create_3
  ASSERT_EQ(ERROR_SUCCESS,
            RegDeleteKey(HKEY_CURRENT_USER, key_to_create_3.value().c_str()));
  ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create_3.value().c_str(), KEY_READ));

  work_item->Rollback();

  // key_to_create_3 has already been deleted, Rollback should delete
  // the rest.
  ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create_1.value().c_str(), KEY_READ));
}

TEST_F(CreateRegKeyWorkItemTest, RollbackWithSetValue) {
  RegKey key;

  base::FilePath key_to_create(test_root);
  key_to_create = key_to_create.AppendASCII("aaaaa");

  std::unique_ptr<CreateRegKeyWorkItem> work_item(
      WorkItem::CreateCreateRegKeyWorkItem(
          HKEY_CURRENT_USER, key_to_create.value(), WorkItem::kWow64Default));

  EXPECT_TRUE(work_item->Do());

  // Write a value under the key we just created.
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_to_create.value().c_str(),
                     KEY_READ | KEY_SET_VALUE));
  EXPECT_EQ(ERROR_SUCCESS, key.WriteValue(L"name", L"value"));
  key.Close();

  work_item->Rollback();

  // Rollback should not remove the key.
  EXPECT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER,
                                    key_to_create.value().c_str(), KEY_READ));
}
