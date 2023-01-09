// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/set_reg_value_work_item.h"

#include <windows.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t kTestKey[] = L"TempTemp";
const wchar_t kDataStr1[] = L"data_111";
const wchar_t kDataStr2[] = L"data_222";
const wchar_t kNameStr[] = L"name_str";
const wchar_t kNameDword[] = L"name_dword";

const DWORD kDword1 = 12345;
const DWORD kDword2 = 6789;

class SetRegValueWorkItemTest : public testing::Test {
 public:
  SetRegValueWorkItemTest(const SetRegValueWorkItemTest&) = delete;
  SetRegValueWorkItemTest& operator=(const SetRegValueWorkItemTest&) = delete;

 protected:
  SetRegValueWorkItemTest() {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    // Create a temporary key for testing.
    ASSERT_NE(ERROR_SUCCESS,
              test_key_.Open(HKEY_CURRENT_USER, kTestKey, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS, test_key_.Create(HKEY_CURRENT_USER, kTestKey,
                                              KEY_READ | KEY_SET_VALUE));
  }

  base::win::RegKey test_key_;

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

}  // namespace

// Write a new value without overwrite flag. The value should be set.
TEST_F(SetRegValueWorkItemTest, WriteNewNonOverwrite) {
  std::unique_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameStr,
                                          kDataStr1, false));

  std::unique_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameDword,
                                          kDword1, false));

  ASSERT_TRUE(work_item1->Do());
  ASSERT_TRUE(work_item2->Do());

  std::wstring read_out;
  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValueDW(kNameDword, &read_dword));
  EXPECT_EQ(read_out, kDataStr1);
  EXPECT_EQ(read_dword, kDword1);

  work_item1->Rollback();
  work_item2->Rollback();

  // Rollback should delete the value.
  EXPECT_FALSE(test_key_.HasValue(kNameStr));
  EXPECT_FALSE(test_key_.HasValue(kNameDword));
}

// Write a new value with overwrite flag. The value should be set.
TEST_F(SetRegValueWorkItemTest, WriteNewOverwrite) {
  std::unique_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameStr,
                                          kDataStr1, true));

  std::unique_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameDword,
                                          kDword1, true));

  ASSERT_TRUE(work_item1->Do());
  ASSERT_TRUE(work_item2->Do());

  std::wstring read_out;
  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValueDW(kNameDword, &read_dword));
  EXPECT_EQ(read_out, kDataStr1);
  EXPECT_EQ(read_dword, kDword1);

  work_item1->Rollback();
  work_item2->Rollback();

  // Rollback should delete the value.
  EXPECT_FALSE(test_key_.HasValue(kNameStr));
  EXPECT_FALSE(test_key_.HasValue(kNameDword));
}

// Write to an existing value without overwrite flag. There should be
// no change.
TEST_F(SetRegValueWorkItemTest, WriteExistingNonOverwrite) {
  // First test REG_SZ value.
  // Write data to the value we are going to set.
  ASSERT_EQ(ERROR_SUCCESS, test_key_.WriteValue(kNameStr, kDataStr1));

  std::unique_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameStr,
                                          kDataStr2, false));
  ASSERT_TRUE(work_item->Do());

  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(read_out, kDataStr1);

  work_item->Rollback();
  EXPECT_TRUE(test_key_.HasValue(kNameStr));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(read_out, kDataStr1);

  // Now test REG_DWORD value.
  // Write data to the value we are going to set.
  ASSERT_EQ(ERROR_SUCCESS, test_key_.WriteValue(kNameDword, kDword1));
  work_item.reset(WorkItem::CreateSetRegValueWorkItem(
      HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameDword, kDword2,
      false));
  ASSERT_TRUE(work_item->Do());

  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValueDW(kNameDword, &read_dword));
  EXPECT_EQ(read_dword, kDword1);

  work_item->Rollback();
  EXPECT_TRUE(test_key_.HasValue(kNameDword));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValueDW(kNameDword, &read_dword));
  EXPECT_EQ(read_dword, kDword1);
}

// Write to an existing value with overwrite flag. The value should be
// overwritten.
TEST_F(SetRegValueWorkItemTest, WriteExistingOverwrite) {
  // First test REG_SZ value.
  // Write data to the value we are going to set.
  ASSERT_EQ(ERROR_SUCCESS, test_key_.WriteValue(kNameStr, kDataStr1));

  const wchar_t kNameEmpty[] = L"name_empty";
  ASSERT_EQ(ERROR_SUCCESS, RegSetValueEx(test_key_.Handle(), kNameEmpty, 0,
                                         REG_SZ, nullptr, 0));

  std::unique_ptr<SetRegValueWorkItem> work_item1(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameStr,
                                          kDataStr2, true));
  std::unique_ptr<SetRegValueWorkItem> work_item2(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameEmpty,
                                          kDataStr2, true));

  ASSERT_TRUE(work_item1->Do());
  ASSERT_TRUE(work_item2->Do());

  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(read_out, kDataStr2);

  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameEmpty, &read_out));
  EXPECT_EQ(read_out, kDataStr2);

  work_item1->Rollback();
  work_item2->Rollback();

  EXPECT_TRUE(test_key_.HasValue(kNameStr));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(read_out, kDataStr1);

  DWORD type = 0;
  DWORD size = 0;
  EXPECT_EQ(ERROR_SUCCESS,
            test_key_.ReadValue(kNameEmpty, nullptr, &size, &type));
  EXPECT_EQ(static_cast<DWORD>(REG_SZ), type);
  EXPECT_EQ(0u, size);

  // Now test REG_DWORD value.
  // Write data to the value we are going to set.
  ASSERT_EQ(ERROR_SUCCESS, test_key_.WriteValue(kNameDword, kDword1));
  std::unique_ptr<SetRegValueWorkItem> work_item3(
      WorkItem::CreateSetRegValueWorkItem(HKEY_CURRENT_USER, kTestKey,
                                          WorkItem::kWow64Default, kNameDword,
                                          kDword2, true));
  ASSERT_TRUE(work_item3->Do());

  DWORD read_dword;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValueDW(kNameDword, &read_dword));
  EXPECT_EQ(read_dword, kDword2);

  work_item3->Rollback();
  EXPECT_TRUE(test_key_.HasValue(kNameDword));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValueDW(kNameDword, &read_dword));
  EXPECT_EQ(read_dword, kDword1);
}

// Write a value to a non-existing key. This should fail.
TEST_F(SetRegValueWorkItemTest, WriteNonExistingKey) {
  std::wstring non_existing(kTestKey);
  non_existing.append(&base::FilePath::kSeparators[0], 1);
  non_existing.append(L"NonExistingKey");

  std::unique_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(
          HKEY_CURRENT_USER, non_existing.c_str(), WorkItem::kWow64Default,
          kNameStr, kDataStr1, false));
  EXPECT_FALSE(work_item->Do());

  work_item.reset(WorkItem::CreateSetRegValueWorkItem(
      HKEY_CURRENT_USER, non_existing.c_str(), WorkItem::kWow64Default,
      kNameStr, kDword1, false));
  EXPECT_FALSE(work_item->Do());
}

// Verifies that |actual_previous_value| is |expected_previous_value| and
// returns |desired_new_value| to replace it. Unconditionally increments
// |invocation_count| to allow tests to assert correctness of the callee's
// behaviour.
std::wstring VerifyPreviousValueAndReplace(
    int* invocation_count,
    const std::wstring& expected_previous_value,
    const std::wstring& desired_new_value,
    const std::wstring& actual_previous_value) {
  ++(*invocation_count);
  EXPECT_EQ(expected_previous_value, actual_previous_value);
  return desired_new_value;
}

// Modify an existing value with the callback API.
TEST_F(SetRegValueWorkItemTest, ModifyExistingWithCallback) {
  // Write |kDataStr1| to the value we are going to modify.
  ASSERT_EQ(ERROR_SUCCESS, test_key_.WriteValue(kNameStr, kDataStr1));

  int callback_invocation_count = 0;

  std::unique_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameStr,
          base::BindOnce(&VerifyPreviousValueAndReplace,
                         &callback_invocation_count, kDataStr1, kDataStr2)));

  // The callback should not be used until the item is executed.
  EXPECT_EQ(0, callback_invocation_count);

  ASSERT_TRUE(work_item->Do());

  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(read_out, kDataStr2);

  // The callback should have been used only once to achieve this result.
  EXPECT_EQ(1, callback_invocation_count);

  work_item->Rollback();

  EXPECT_TRUE(test_key_.HasValue(kNameStr));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(read_out, kDataStr1);

  // The callback should not have been used again for the rollback.
  EXPECT_EQ(1, callback_invocation_count);
}

// Modify a non-existing value with the callback API.
TEST_F(SetRegValueWorkItemTest, ModifyNonExistingWithCallback) {
  int callback_invocation_count = 0;

  std::unique_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameStr,
          base::BindOnce(&VerifyPreviousValueAndReplace,
                         &callback_invocation_count, L"", kDataStr1)));

  EXPECT_EQ(0, callback_invocation_count);

  ASSERT_TRUE(work_item->Do());

  std::wstring read_out;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_out));
  EXPECT_EQ(read_out, kDataStr1);

  EXPECT_EQ(1, callback_invocation_count);

  work_item->Rollback();

  EXPECT_FALSE(test_key_.HasValue(kNameStr));

  EXPECT_EQ(1, callback_invocation_count);
}

// Modify an existing value which is not a string (REG_SZ) with the string
// callback API.
TEST_F(SetRegValueWorkItemTest, ModifyExistingNonStringWithStringCallback) {
  // Write |kDword1| to the value we are going to modify.
  ASSERT_EQ(ERROR_SUCCESS, test_key_.WriteValue(kNameStr, kDword1));

  int callback_invocation_count = 0;

  std::unique_ptr<SetRegValueWorkItem> work_item(
      WorkItem::CreateSetRegValueWorkItem(
          HKEY_CURRENT_USER, kTestKey, WorkItem::kWow64Default, kNameStr,
          base::BindOnce(&VerifyPreviousValueAndReplace,
                         &callback_invocation_count, L"", kDataStr1)));

  EXPECT_EQ(0, callback_invocation_count);

  ASSERT_TRUE(work_item->Do());

  std::wstring read_str_out;
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValue(kNameStr, &read_str_out));
  EXPECT_EQ(read_str_out, kDataStr1);

  EXPECT_EQ(1, callback_invocation_count);

  work_item->Rollback();

  DWORD read_dword_out = 0;
  EXPECT_TRUE(test_key_.HasValue(kNameStr));
  EXPECT_EQ(ERROR_SUCCESS, test_key_.ReadValueDW(kNameStr, &read_dword_out));
  EXPECT_EQ(read_dword_out, kDword1);

  EXPECT_EQ(1, callback_invocation_count);
}
