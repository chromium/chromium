// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/registry_util.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;
using ::testing::_;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrEq;

namespace installer {

class MockRegistryValuePredicate : public RegistryValuePredicate {
 public:
  MOCK_CONST_METHOD1(Evaluate, bool(const std::wstring&));
};

class RegistryUtilTest : public testing::Test {
 public:
  RegistryUtilTest(const RegistryUtilTest&) = delete;
  RegistryUtilTest& operator=(const RegistryUtilTest&) = delete;

 protected:
  RegistryUtilTest() {}

  void SetUp() override { ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides()); }

  void ResetRegistryOverrides() {
    registry_override_manager_ =
        std::make_unique<registry_util::RegistryOverrideManager>();
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_->OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_->OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

 private:
  std::unique_ptr<registry_util::RegistryOverrideManager>
      registry_override_manager_;
};

TEST_F(RegistryUtilTest, DeleteRegistryKeyIf) {
  const HKEY root = HKEY_CURRENT_USER;
  std::wstring parent_key_path(L"SomeKey\\ToDelete");
  std::wstring child_key_path(parent_key_path);
  child_key_path.append(L"\\ChildKey\\WithAValue");
  const wchar_t value_name[] = L"some_value_name";
  const wchar_t value[] = L"hi mom";

  // Nothing to delete if the keys aren't even there.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(_)).Times(0);
    ASSERT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
    EXPECT_EQ(ConditionalDeleteResult::NOT_FOUND,
              DeleteRegistryKeyIf(root, parent_key_path, child_key_path,
                                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Parent exists, but not child: no delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(_)).Times(0);
    ASSERT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_SET_VALUE).Valid());
    EXPECT_EQ(ConditionalDeleteResult::NOT_FOUND,
              DeleteRegistryKeyIf(root, parent_key_path, child_key_path,
                                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Child exists, but no value: no delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(_)).Times(0);
    ASSERT_TRUE(RegKey(root, child_key_path.c_str(), KEY_SET_VALUE).Valid());
    EXPECT_EQ(ConditionalDeleteResult::NOT_FOUND,
              DeleteRegistryKeyIf(root, parent_key_path, child_key_path,
                                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Value exists, but doesn't match: no delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(StrEq(L"foosball!"))).WillOnce(Return(false));
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root, child_key_path.c_str(), KEY_SET_VALUE)
                                 .WriteValue(value_name, L"foosball!"));
    EXPECT_EQ(ConditionalDeleteResult::NOT_FOUND,
              DeleteRegistryKeyIf(root, parent_key_path, child_key_path,
                                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Value exists, and matches: delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root, child_key_path.c_str(), KEY_SET_VALUE)
                                 .WriteValue(value_name, value));
    EXPECT_EQ(ConditionalDeleteResult::DELETED,
              DeleteRegistryKeyIf(root, parent_key_path, child_key_path,
                                  WorkItem::kWow64Default, value_name, pred));
    EXPECT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }

  // Default value exists and matches: delete.
  {
    MockRegistryValuePredicate pred;

    EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
    ASSERT_EQ(ERROR_SUCCESS, RegKey(root, child_key_path.c_str(), KEY_SET_VALUE)
                                 .WriteValue(nullptr, value));
    EXPECT_EQ(ConditionalDeleteResult::DELETED,
              DeleteRegistryKeyIf(root, parent_key_path, child_key_path,
                                  WorkItem::kWow64Default, nullptr, pred));
    EXPECT_FALSE(
        RegKey(root, parent_key_path.c_str(), KEY_QUERY_VALUE).Valid());
  }
}

TEST_F(RegistryUtilTest, DeleteRegistryValueIf) {
  const HKEY root = HKEY_CURRENT_USER;
  std::wstring key_path(L"SomeKey\\ToDelete");
  const wchar_t value_name[] = L"some_value_name";
  const wchar_t value[] = L"hi mom";

  {
    ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides());
    // Nothing to delete if the key isn't even there.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_FALSE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_EQ(
          ConditionalDeleteResult::NOT_FOUND,
          DeleteRegistryValueIf(root, key_path.c_str(), WorkItem::kWow64Default,
                                value_name, pred));
      EXPECT_FALSE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
    }

    // Key exists, but no value: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_TRUE(RegKey(root, key_path.c_str(), KEY_SET_VALUE).Valid());
      EXPECT_EQ(
          ConditionalDeleteResult::NOT_FOUND,
          DeleteRegistryValueIf(root, key_path.c_str(), WorkItem::kWow64Default,
                                value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
    }

    // Value exists, but doesn't match: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(L"foosball!"))).WillOnce(Return(false));
      ASSERT_EQ(ERROR_SUCCESS, RegKey(root, key_path.c_str(), KEY_SET_VALUE)
                                   .WriteValue(value_name, L"foosball!"));
      EXPECT_EQ(
          ConditionalDeleteResult::NOT_FOUND,
          DeleteRegistryValueIf(root, key_path.c_str(), WorkItem::kWow64Default,
                                value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_TRUE(
          RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).HasValue(value_name));
    }

    // Value exists, and matches: delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(ERROR_SUCCESS, RegKey(root, key_path.c_str(), KEY_SET_VALUE)
                                   .WriteValue(value_name, value));
      EXPECT_EQ(
          ConditionalDeleteResult::DELETED,
          DeleteRegistryValueIf(root, key_path.c_str(), WorkItem::kWow64Default,
                                value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(
          RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).HasValue(value_name));
    }
  }

  {
    ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides());
    // Default value matches: delete using empty string.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(
          ERROR_SUCCESS,
          RegKey(root, key_path.c_str(), KEY_SET_VALUE).WriteValue(L"", value));
      EXPECT_EQ(ConditionalDeleteResult::DELETED,
                DeleteRegistryValueIf(root, key_path.c_str(),
                                      WorkItem::kWow64Default, L"", pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(
          RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).HasValue(L""));
    }
  }

  {
    ASSERT_NO_FATAL_FAILURE(ResetRegistryOverrides());
    // Default value matches: delete using nullptr.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(
          ERROR_SUCCESS,
          RegKey(root, key_path.c_str(), KEY_SET_VALUE).WriteValue(L"", value));
      EXPECT_EQ(ConditionalDeleteResult::DELETED,
                DeleteRegistryValueIf(root, key_path.c_str(),
                                      WorkItem::kWow64Default, nullptr, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(
          RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).HasValue(L""));
    }
  }
}

TEST_F(RegistryUtilTest, ValueEquals) {
  ValueEquals pred(L"howdy");

  EXPECT_FALSE(pred.Evaluate(L""));
  EXPECT_FALSE(pred.Evaluate(L"Howdy"));
  EXPECT_FALSE(pred.Evaluate(L"howdy!"));
  EXPECT_FALSE(pred.Evaluate(L"!howdy"));
  EXPECT_TRUE(pred.Evaluate(L"howdy"));
}

// A matcher that returns true if its argument (a string) matches a given
// base::FilePath.
MATCHER_P(EqPathIgnoreCase, value, "") {
  return base::FilePath::CompareEqualIgnoreCase(arg, value.value());
}

TEST_F(RegistryUtilTest, ProgramCompare) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  const base::FilePath some_long_dir(
      test_dir.GetPath().Append(L"Some Long Directory Name"));
  const base::FilePath expect(some_long_dir.Append(L"file.txt"));
  const base::FilePath expect_upcase(some_long_dir.Append(L"FILE.txt"));
  const base::FilePath other(some_long_dir.Append(L"otherfile.txt"));

  // Tests where the expected file doesn't exist.

  // Paths don't match.
  EXPECT_FALSE(ProgramCompare(expect).Evaluate(L"\"" + other.value() + L"\""));
  // Paths match exactly.
  EXPECT_TRUE(ProgramCompare(expect).Evaluate(L"\"" + expect.value() + L"\""));
  // Paths differ by case.
  EXPECT_TRUE(
      ProgramCompare(expect).Evaluate(L"\"" + expect_upcase.value() + L"\""));

  // Tests where the expected file exists.
  static const char data[] = "data";
  ASSERT_TRUE(base::CreateDirectory(some_long_dir));
  ASSERT_TRUE(base::WriteFile(expect, data));
  // Paths don't match.
  EXPECT_FALSE(ProgramCompare(expect).Evaluate(L"\"" + other.value() + L"\""));
  // Paths match exactly.
  EXPECT_TRUE(ProgramCompare(expect).Evaluate(L"\"" + expect.value() + L"\""));
  // Paths differ by case.
  EXPECT_TRUE(
      ProgramCompare(expect).Evaluate(L"\"" + expect_upcase.value() + L"\""));

  // Test where strings don't match, but the same file is indicated.
  std::wstring short_expect;
  DWORD short_len =
      GetShortPathName(expect.value().c_str(),
                       base::WriteInto(&short_expect, MAX_PATH), MAX_PATH);
  ASSERT_NE(static_cast<DWORD>(0), short_len);
  ASSERT_GT(static_cast<DWORD>(MAX_PATH), short_len);
  short_expect.resize(short_len);
  // GetShortPathName may return the original path in case there is no short
  // form. Only perform the last expectation if the short form was found.
  if (!base::FilePath::CompareEqualIgnoreCase(expect.value(), short_expect)) {
    EXPECT_TRUE(ProgramCompare(expect).Evaluate(L"\"" + short_expect + L"\""));
  }
}

}  // namespace installer
