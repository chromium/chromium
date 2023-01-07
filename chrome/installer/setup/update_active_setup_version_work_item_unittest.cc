// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/update_active_setup_version_work_item.h"

#include <windows.h>

#include <ostream>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ValuesIn;

namespace {

const HKEY kActiveSetupRoot = HKEY_LOCAL_MACHINE;
const wchar_t kActiveSetupPath[] = L"Active Setup\\test";

struct UpdateActiveSetupVersionWorkItemTestCase {
  // The initial value to be set in the registry prior to executing the
  // UpdateActiveSetupVersionWorkItem. No value will be set if this null.
  const wchar_t* initial_value;

  // Whether to ask the UpdateActiveSetupVersionWorkItem to bump the
  // SELECTIVE_TRIGGER component of the Active Setup version.
  bool bump_selective_trigger;

  // The expected value after executing the UpdateActiveSetupVersionWorkItem.
  const wchar_t* expected_result;
} const kUpdateActiveSetupVersionWorkItemTestCases[] = {
    // Initial install.
    {nullptr, false, L"43,0,0,0"},
    // No-op update.
    {L"43,0,0,0", false, L"43,0,0,0"},
    // Update only major component.
    {L"24,1,2,3", false, L"43,0,0,0"},
    // Reset from bogus value.
    {L"zzz", false, L"43,0,0,0"},
    // Reset from invalid version (too few components).
    {L"1,2", false, L"43,0,0,0"},
    // Reset from invalid version (too many components).
    {L"43,1,2,3,10", false, L"43,1,2,3"},
    // Reset from empty string.
    {L"", false, L"43,0,0,0"},

    // Same tests with a SELECTIVE_TRIGGER component bump.
    {nullptr, true, L"43,0,0,0"},
    {L"43,0,0,0", true, L"43,0,1,0"},
    {L"43,0,46,0", true, L"43,0,47,0"},
    {L"24,1,2,3", true, L"43,0,0,0"},
    {L"zzz", true, L"43,0,0,0"},
    {L"1,2", true, L"43,0,0,0"},
    {L"43,1,2,3,10", true, L"43,1,3,3"},
    {L"", true, L"43,0,0,0"},
    // Bumping a negative selective trigger component should result in it being
    // reset and subsequently bumped to 1.
    {L"43,11,-123,33", true, L"43,11,1,33"},
};

// Implements PrintTo in order for gtest to be able to print the problematic
// UpdateActiveSetupVersionWorkItemTestCase on failure.
void PrintTo(const UpdateActiveSetupVersionWorkItemTestCase& test_case,
             ::std::ostream* os) {
  *os << "Initial value: "
      << (test_case.initial_value ? base::WideToUTF8(test_case.initial_value)
                                  : "(empty)")
      << ", bump_selective_trigger: " << test_case.bump_selective_trigger
      << ", expected result: " << base::WideToUTF8(test_case.expected_result);
}

}  // namespace

class UpdateActiveSetupVersionWorkItemTest
    : public testing::TestWithParam<UpdateActiveSetupVersionWorkItemTestCase> {
 public:
  UpdateActiveSetupVersionWorkItemTest() {}

  UpdateActiveSetupVersionWorkItemTest(
      const UpdateActiveSetupVersionWorkItemTest&) = delete;
  UpdateActiveSetupVersionWorkItemTest& operator=(
      const UpdateActiveSetupVersionWorkItemTest&) = delete;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(kActiveSetupRoot));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_P(UpdateActiveSetupVersionWorkItemTest, Execute) {
  // Get the parametrized |test_case| which defines 5 steps:
  //   1) Maybe set an initial Active Setup version in the registry according to
  //      |test_case.initial_value|.
  //   2) Declare the work to be done by the UpdateActiveSetupVersionWorkItem
  //      based on |test_case.bump_selective_trigger|.
  //   3) Unconditionally execute the Active Setup work items.
  //   4) Verify that the updated Active Setup version is as expected by
  //      |test_case.expected_result|.
  //   5) Rollback and verify that |test_case.initial_value| is back.
  const UpdateActiveSetupVersionWorkItemTestCase& test_case = GetParam();

  base::win::RegKey test_key;

  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            test_key.Open(kActiveSetupRoot, kActiveSetupPath, KEY_READ));

  UpdateActiveSetupVersionWorkItem active_setup_work_item(
      kActiveSetupPath,
      test_case.bump_selective_trigger
          ? UpdateActiveSetupVersionWorkItem::UPDATE_AND_BUMP_SELECTIVE_TRIGGER
          : UpdateActiveSetupVersionWorkItem::UPDATE);

  // Create the key and set the |initial_value| *after* the WorkItem to confirm
  // that all of the work is done when executing the item, not when creating it.
  ASSERT_EQ(ERROR_SUCCESS,
            test_key.Create(kActiveSetupRoot, kActiveSetupPath, KEY_SET_VALUE));
  if (test_case.initial_value) {
    ASSERT_EQ(ERROR_SUCCESS,
              test_key.WriteValue(L"Version", test_case.initial_value));
  }

  EXPECT_TRUE(active_setup_work_item.Do());

  {
    std::wstring version_out;
    EXPECT_EQ(ERROR_SUCCESS, test_key.Open(kActiveSetupRoot, kActiveSetupPath,
                                           KEY_QUERY_VALUE));
    EXPECT_EQ(ERROR_SUCCESS, test_key.ReadValue(L"Version", &version_out));
    EXPECT_EQ(test_case.expected_result, version_out);
  }

  active_setup_work_item.Rollback();

  {
    EXPECT_EQ(ERROR_SUCCESS, test_key.Open(kActiveSetupRoot, kActiveSetupPath,
                                           KEY_QUERY_VALUE));

    std::wstring version_out;
    LONG read_result = test_key.ReadValue(L"Version", &version_out);
    if (test_case.initial_value) {
      EXPECT_EQ(ERROR_SUCCESS, read_result);
      EXPECT_EQ(test_case.initial_value, version_out);
    } else {
      EXPECT_EQ(ERROR_FILE_NOT_FOUND, read_result);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(UpdateActiveSetupVersionWorkItemTestInstance,
                         UpdateActiveSetupVersionWorkItemTest,
                         ValuesIn(kUpdateActiveSetupVersionWorkItemTestCases));
