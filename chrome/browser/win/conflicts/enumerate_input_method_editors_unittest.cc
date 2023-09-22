// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/enumerate_input_method_editors.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class EnumerateInputMethodEditorsTest : public testing::Test {
 public:
  EnumerateInputMethodEditorsTest(const EnumerateInputMethodEditorsTest&) =
      delete;
  EnumerateInputMethodEditorsTest& operator=(
      const EnumerateInputMethodEditorsTest&) = delete;

 protected:
  EnumerateInputMethodEditorsTest() = default;
  ~EnumerateInputMethodEditorsTest() override = default;

  // Override all registry hives so that real IMEs don't mess up the unit tests.
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CLASSES_ROOT));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  registry_util::RegistryOverrideManager registry_override_manager_;
};

// Adds a fake IME entry to the registry that should be found by the
// enumeration. The call must be wrapped inside an ASSERT_NO_FATAL_FAILURE.
void RegisterFakeIme(const wchar_t* guid, const wchar_t* path) {
  base::win::RegKey class_id(HKEY_CLASSES_ROOT, GuidToClsid(guid).c_str(),
                             KEY_WRITE);
  ASSERT_TRUE(class_id.Valid());

  ASSERT_EQ(ERROR_SUCCESS, class_id.WriteValue(nullptr, path));

  base::win::RegKey registration(HKEY_LOCAL_MACHINE, kImeRegistryKey,
                                 KEY_WRITE);
  ASSERT_EQ(ERROR_SUCCESS, registration.CreateKey(guid, KEY_WRITE));
}

void OnImeEnumerated(std::vector<base::FilePath>* imes,
                     const base::FilePath& ime_path,
                     uint32_t size_of_image,
                     uint32_t time_date_stamp) {
  imes->push_back(ime_path);
}

void OnEnumerationFinished(bool* is_enumeration_finished) {
  *is_enumeration_finished = true;
}

}  // namespace

// Registers a few fake IMEs then see if the enumeration finds them.
TEST_F(EnumerateInputMethodEditorsTest, EnumerateImes) {
  // Use the current exe file as an arbitrary module that exists.
  base::FilePath file_exe;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));
  ASSERT_NO_FATAL_FAILURE(
      RegisterFakeIme(L"{FAKE_GUID}", file_exe.value().c_str()));

  // Do the asynchronous enumeration.
  std::vector<base::FilePath> imes;
  bool is_enumeration_finished = false;
  EnumerateInputMethodEditors(
      base::BindRepeating(&OnImeEnumerated, base::Unretained(&imes)),
      base::BindOnce(&OnEnumerationFinished,
                     base::Unretained(&is_enumeration_finished)));

  RunUntilIdle();

  EXPECT_TRUE(is_enumeration_finished);
  ASSERT_EQ(1u, imes.size());
  EXPECT_EQ(file_exe, imes[0]);
}

TEST_F(EnumerateInputMethodEditorsTest, SkipMicrosoftImes) {
  static constexpr wchar_t kMicrosoftImeExample[] =
      L"{6a498709-e00b-4c45-a018-8f9e4081ae40}";

  // Register a fake IME using the Microsoft IME guid.
  ASSERT_NO_FATAL_FAILURE(
      RegisterFakeIme(kMicrosoftImeExample, L"c:\\path\\to\\ime.dll"));

  // Do the asynchronous enumeration.
  std::vector<base::FilePath> imes;
  bool is_enumeration_finished = false;
  EnumerateInputMethodEditors(
      base::BindRepeating(&OnImeEnumerated, base::Unretained(&imes)),
      base::BindOnce(&OnEnumerationFinished,
                     base::Unretained(&is_enumeration_finished)));

  RunUntilIdle();

  EXPECT_TRUE(is_enumeration_finished);
  EXPECT_TRUE(imes.empty());
}
