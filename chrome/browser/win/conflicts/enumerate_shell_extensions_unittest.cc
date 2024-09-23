// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/enumerate_shell_extensions.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/strcat_win.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class EnumerateShellExtensionsTest : public testing::Test {
 public:
  EnumerateShellExtensionsTest() = default;

  EnumerateShellExtensionsTest(const EnumerateShellExtensionsTest&) = delete;
  EnumerateShellExtensionsTest& operator=(const EnumerateShellExtensionsTest&) =
      delete;

  ~EnumerateShellExtensionsTest() override = default;

  // Override all registry hives so that real shell extensions don't mess up
  // the unit tests.
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

// Adds a fake shell extension entry to the registry that should be found by
// the ShellExtensionEnumerator. The call must be wrapped inside an
// ASSERT_NO_FATAL_FAILURE.
void RegisterFakeApprovedShellExtension(HKEY key,
                                        const wchar_t* guid,
                                        const wchar_t* path) {
  base::win::RegKey class_id(HKEY_CLASSES_ROOT, GuidToClsid(guid).c_str(),
                             KEY_WRITE);
  ASSERT_TRUE(class_id.Valid());

  ASSERT_EQ(ERROR_SUCCESS, class_id.WriteValue(nullptr, path));

  base::win::RegKey registration(key, kApprovedShellExtensionRegistryKey,
                                 KEY_WRITE);
  ASSERT_EQ(ERROR_SUCCESS, registration.WriteValue(guid, L""));
}

// Adds a fake shell extension entry to the registry that should be found by
// the ShellExtensionEnumerator. The call must be wrapped inside an
// ASSERT_NO_FATAL_FAILURE.
void RegisterFakeShellExtension(const wchar_t* guid,
                                const wchar_t* path,
                                const wchar_t* shell_extension_type,
                                const wchar_t* shell_object_type) {
  base::win::RegKey class_id(HKEY_CLASSES_ROOT, GuidToClsid(guid).c_str(),
                             KEY_WRITE);
  ASSERT_TRUE(class_id.Valid());

  ASSERT_EQ(ERROR_SUCCESS, class_id.WriteValue(nullptr, path));

  base::win::RegKey registration(
      HKEY_CLASSES_ROOT,
      base::StrCat({shell_object_type, L"\\shellex\\", shell_extension_type,
                    L"\\", guid})
          .c_str(),
      KEY_WRITE);
  ASSERT_EQ(ERROR_SUCCESS, registration.WriteValue(nullptr, guid));
}

void OnShellExtensionPathEnumerated(
    std::vector<base::FilePath>* shell_extension_paths,
    const base::FilePath& path) {
  shell_extension_paths->push_back(path);
}

void OnShellExtensionEnumerated(
    std::vector<base::FilePath>* shell_extension_paths,
    const base::FilePath& path,
    uint32_t size_of_image,
    uint32_t time_date_stamp) {
  shell_extension_paths->push_back(path);
}

void OnEnumerationFinished(bool* is_enumeration_finished) {
  *is_enumeration_finished = true;
}

}  // namespace

// Registers a few fake approved shell extensions then see if
// EnumerateShellExtensionPaths() finds them.
TEST_F(EnumerateShellExtensionsTest, EnumerateApprovedShellExtensionPaths) {
  struct {
    HKEY hkey_root;
    const wchar_t* guid;
    const wchar_t* path;
  } kTestCases[] = {
      {HKEY_CURRENT_USER, L"{FAKE_GUID_0001}", L"c:\\module.dll"},
      {HKEY_LOCAL_MACHINE, L"{FAKE_GUID_0002}", L"c:\\dir\\shell_ext.dll"},
      {HKEY_LOCAL_MACHINE, L"{FAKE_GUID_0003}", L"c:\\path\\test.dll"},
  };

  // Register all fake shell extensions in kTestCases.
  for (const auto& test_case : kTestCases) {
    ASSERT_NO_FATAL_FAILURE(RegisterFakeApprovedShellExtension(
        test_case.hkey_root, test_case.guid, test_case.path));
  }

  std::vector<base::FilePath> shell_extension_paths;
  internal::EnumerateShellExtensionPaths(
      base::BindRepeating(&OnShellExtensionPathEnumerated,
                          base::Unretained(&shell_extension_paths)));

  ASSERT_EQ(3u, shell_extension_paths.size());
  for (const auto& test_case : kTestCases) {
    // The inefficiency is fine as long as the number of test cases stays small.
    EXPECT_TRUE(
        base::Contains(shell_extension_paths, base::FilePath(test_case.path)));
  }
}

TEST_F(EnumerateShellExtensionsTest, EnumerateApprovedShellExtensions) {
  // Use the current exe file as an arbitrary module that exists.
  base::FilePath file_exe;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));
  ASSERT_NO_FATAL_FAILURE(RegisterFakeApprovedShellExtension(
      HKEY_LOCAL_MACHINE, L"{FAKE_GUID}", file_exe.value().c_str()));

  std::vector<base::FilePath> shell_extension_paths;
  bool is_enumeration_finished = false;
  EnumerateShellExtensions(
      base::BindRepeating(&OnShellExtensionEnumerated,
                          base::Unretained(&shell_extension_paths)),
      base::BindOnce(&OnEnumerationFinished,
                     base::Unretained(&is_enumeration_finished)));

  RunUntilIdle();

  EXPECT_TRUE(is_enumeration_finished);
  ASSERT_EQ(1u, shell_extension_paths.size());
  EXPECT_EQ(file_exe, shell_extension_paths[0]);
}

TEST_F(EnumerateShellExtensionsTest, EnumerateShellExtensionPaths) {
  struct {
    const wchar_t* guid;
    const wchar_t* path;
    const wchar_t* shell_extension_type;
    const wchar_t* shell_object_type;
  } kTestCases[] = {
      {L"{FAKE_GUID_0001}", L"c:\\module.dll", L"ColumnHandlers", L"Folder"},
      {L"{FAKE_GUID_0002}", L"c:\\dir\\shell_ext.dll", L"ContextMenuHandlers",
       L"*"},
      {L"{FAKE_GUID_0003}", L"c:\\path\\test.dll", L"CopyHookHandlers",
       L"Printers"},
      {L"{FAKE_GUID_0004}", L"c:\\foo\\bar.dll", L"DragDropHandlers", L"Drive"},
      {L"{FAKE_GUID_0005}", L"c:\\foo\\baz.dll", L"PropertySheetHandlers",
       L"AllFileSystemObjects"},
  };

  // Register all fake shell extensions in kTestCases.
  for (const auto& test_case : kTestCases) {
    ASSERT_NO_FATAL_FAILURE(RegisterFakeShellExtension(
        test_case.guid, test_case.path, test_case.shell_extension_type,
        test_case.shell_object_type));
  }

  std::vector<base::FilePath> shell_extension_paths;
  internal::EnumerateShellExtensionPaths(
      base::BindRepeating(&OnShellExtensionPathEnumerated,
                          base::Unretained(&shell_extension_paths)));

  ASSERT_EQ(5u, shell_extension_paths.size());
  for (const auto& test_case : kTestCases) {
    // The inefficiency is fine as long as the number of test cases stays small.
    EXPECT_TRUE(
        base::Contains(shell_extension_paths, base::FilePath(test_case.path)));
  }
}
