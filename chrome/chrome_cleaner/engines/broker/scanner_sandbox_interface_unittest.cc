// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/scanner_sandbox_interface.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/engines/common/registry_util.h"
#include "chrome/chrome_cleaner/engines/common/sandbox_error_code.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/test/scoped_process_protector.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_native_reg_util.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_task_scheduler.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sid.h"
#include "sandbox/win/src/win_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome_cleaner::GetWow64RedirectedSystemPath;
using chrome_cleaner::SandboxErrorCode;
using chrome_cleaner::ScopedTempDirNoWow64;
using chrome_cleaner::String16EmbeddedNulls;

namespace chrome_cleaner_sandbox {

namespace {

using KnownFolder = chrome_cleaner::mojom::KnownFolder;

#define STATUS_OBJECT_PATH_SYNTAX_BAD ((NTSTATUS)0xC000003BL)

String16EmbeddedNulls StringWithTrailingNull(const base::string16& str) {
  // string16::size() does not count the trailing null.
  return String16EmbeddedNulls(str.c_str(), str.size() + 1);
}

class ScopedCurrentDirectory {
 public:
  explicit ScopedCurrentDirectory(const base::FilePath& new_directory) {
    CHECK(base::GetCurrentDirectory(&original_directory_));
    CHECK(base::SetCurrentDirectory(new_directory));
  }

  ~ScopedCurrentDirectory() {
    CHECK(base::SetCurrentDirectory(original_directory_));
  }

 private:
  base::FilePath original_directory_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCurrentDirectory);
};

bool operator!=(const chrome_cleaner::TaskScheduler::TaskExecAction& left,
                const chrome_cleaner::TaskScheduler::TaskExecAction& right) {
  return left.application_path != right.application_path ||
         left.working_dir != right.working_dir ||
         left.arguments != right.arguments;
}

base::FilePath GetNativePath(const base::string16& path) {
  // Add the native path \??\ prefix described at
  // https://googleprojectzero.blogspot.com/2016/02/the-definitive-guide-on-win32-to-nt.html
  return base::FilePath(base::StrCat({L"\\??\\", path}));
}

base::FilePath GetUniversalPath(const base::string16& path) {
  // Add the universal \\?\ prefix described at
  // https://docs.microsoft.com/en-us/windows/desktop/fileio/naming-a-file#namespaces
  return base::FilePath(base::StrCat({L"\\\\?\\", path}));
}

bool CreateNetworkedFile(const base::FilePath& path) {
  chrome_cleaner::CreateEmptyFile(path);

  // Fake an attribute that would be present on a networked file.
  // FILE_ATTRIBUTE_OFFLINE was chosen as FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
  // and FILE_ATTRIBUTE_RECALL_ON_OPEN do not seem to be user-settable.
  const DWORD attr = ::GetFileAttributes(path.value().c_str());
  return ::SetFileAttributesW(path.value().c_str(),
                              attr | FILE_ATTRIBUTE_OFFLINE);
}

}  // namespace

TEST(ScannerSandboxInterface, FindFirstFile_OneFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.txt");

  ASSERT_TRUE(chrome_cleaner::CreateEmptyFile(file_path));

  base::string16 search_pattern = L"temp*";
  base::FilePath search_path = temp.GetPath().Append(search_pattern);

  HANDLE handle;
  WIN32_FIND_DATAW data;
  EXPECT_EQ(0U, SandboxFindFirstFile(search_path, &data, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  EXPECT_STREQ(file_path.BaseName().value().c_str(), data.cFileName);

  EXPECT_EQ(static_cast<uint32_t>(ERROR_NO_MORE_FILES),
            SandboxFindNextFile(handle, &data));

  EXPECT_EQ(0U, SandboxFindClose(handle));

  // Make sure that the file can't be given as a relative path
  ScopedCurrentDirectory directory_override(temp.GetPath());

  EXPECT_EQ(
      SandboxErrorCode::INVALID_FILE_PATH,
      SandboxFindFirstFile(base::FilePath(search_pattern), &data, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);

  // Make sure that the file path requires a drive letter.
  base::FilePath native_path = GetNativePath(search_pattern);
  EXPECT_EQ(SandboxErrorCode::INVALID_FILE_PATH,
            SandboxFindFirstFile(native_path, &data, &handle))
      << native_path;
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle) << native_path;

  base::FilePath universal_path = GetUniversalPath(search_pattern);
  EXPECT_EQ(SandboxErrorCode::INVALID_FILE_PATH,
            SandboxFindFirstFile(universal_path, &data, &handle))
      << universal_path;
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle) << universal_path;
}

TEST(ScannerSandboxInterface, FindNextFile_MultipleFiles) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path_1 = temp.GetPath().Append(L"temp_file_1.txt");
  base::FilePath file_path_2 = temp.GetPath().Append(L"temp_file_2.txt");

  ASSERT_TRUE(chrome_cleaner::CreateEmptyFile(file_path_1));
  ASSERT_TRUE(chrome_cleaner::CreateEmptyFile(file_path_2));

  HANDLE handle;
  base::FilePath search_path = temp.GetPath().Append(L"temp*");
  WIN32_FIND_DATAW data;

  EXPECT_EQ(0U, SandboxFindFirstFile(search_path, &data, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  base::string16 first_found = data.cFileName;
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(file_path_1.BaseName().value(),
                                               data.cFileName) ||
              base::EqualsCaseInsensitiveASCII(file_path_2.BaseName().value(),
                                               data.cFileName))
      << "Returned file name doesn't match, expected "
      << file_path_1.BaseName().value() << " or "
      << file_path_2.BaseName().value() << " and got " << data.cFileName;

  EXPECT_EQ(0U, SandboxFindNextFile(handle, &data));
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(file_path_1.BaseName().value(),
                                               data.cFileName) ||
              base::EqualsCaseInsensitiveASCII(file_path_2.BaseName().value(),
                                               data.cFileName))
      << "Returned file name doesn't match, expected "
      << file_path_1.BaseName().value() << " or "
      << file_path_2.BaseName().value() << " and got " << data.cFileName;
  EXPECT_STRNE(first_found.c_str(), data.cFileName)
      << "Same file name was returned twice. " << first_found;

  EXPECT_EQ(static_cast<uint32_t>(ERROR_NO_MORE_FILES),
            SandboxFindNextFile(handle, &data));

  EXPECT_EQ(0U, SandboxFindClose(handle));
}

TEST(ScannerSandboxInterface, FindFirstFile_InvalidInputs) {
  HANDLE handle;
  WIN32_FIND_DATAW data;

  EXPECT_EQ(SandboxErrorCode::INVALID_FILE_PATH,
            SandboxFindFirstFile(base::FilePath(), &data, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST(ScannerSandboxInterface, FindFirstFile_NoMatchingFiles) {
  HANDLE handle;
  WIN32_FIND_DATAW data;
  const base::FilePath file_path(L"C:/fake/path/that/I/made/up.txt");

  EXPECT_EQ(static_cast<uint32_t>(ERROR_PATH_NOT_FOUND),
            SandboxFindFirstFile(file_path, &data, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST(ScannerSandboxInterface, FindFirstFile_Wow64Disabled) {
  static constexpr wchar_t kTestFile[] = L"temp_file.txt";

  ScopedTempDirNoWow64 temp_dir;
  ASSERT_TRUE(temp_dir.CreateEmptyFileInUniqueSystem32TempDir(kTestFile));
  base::FilePath base_path = temp_dir.GetPath().BaseName();

  // Make sure the file can be found in the system32 directory.
  base::FilePath system_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &system_path));
  base::FilePath search_path = system_path.Append(base_path).Append(L"temp*");

  {
    SCOPED_TRACE(::testing::Message() << "search_path = " << search_path);
    HANDLE handle;
    WIN32_FIND_DATAW data;
    EXPECT_EQ(0U, SandboxFindFirstFile(search_path, &data, &handle));
    EXPECT_NE(INVALID_HANDLE_VALUE, handle);
    base::string16 first_found = data.cFileName;
    EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(kTestFile, data.cFileName));

    EXPECT_EQ(static_cast<uint32_t>(ERROR_NO_MORE_FILES),
              SandboxFindNextFile(handle, &data));
    EXPECT_EQ(0U, SandboxFindClose(handle));
  }

  // Make sure the file is NOT found in the redirected directory. Skip this
  // test on 32-bit Windows because the redirected path will be empty as Wow64
  // redirection is not supported.
  if (chrome_cleaner::IsX64Architecture()) {
    base::FilePath redirected_path = GetWow64RedirectedSystemPath();
    ASSERT_FALSE(redirected_path.empty());
    ASSERT_NE(redirected_path, system_path);
    search_path = redirected_path.Append(base_path).Append(L"temp*");

    SCOPED_TRACE(::testing::Message() << "search_path = " << search_path);
    HANDLE handle;
    WIN32_FIND_DATAW data;
    EXPECT_EQ(static_cast<uint32_t>(ERROR_PATH_NOT_FOUND),
              SandboxFindFirstFile(search_path, &data, &handle));
  }
}

TEST(ScannerSandboxInterface, FindFirstFile_WithNetworkedFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file_1.txt");
  ASSERT_TRUE(CreateNetworkedFile(file_path));

  HANDLE handle;
  WIN32_FIND_DATAW data;
  EXPECT_EQ(static_cast<uint32_t>(ERROR_NO_MORE_FILES),
            SandboxFindFirstFile(file_path, &data, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST(ScannerSandboxInterface, FindNextFile_WithNetworkedFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path_1 = temp.GetPath().Append(L"temp_file_1.txt");
  base::FilePath file_path_2 = temp.GetPath().Append(L"temp_file_2.txt");

  ASSERT_TRUE(chrome_cleaner::CreateEmptyFile(file_path_1));
  ASSERT_TRUE(CreateNetworkedFile(file_path_2));

  HANDLE handle;
  base::FilePath search_path = temp.GetPath().Append(L"temp*");
  WIN32_FIND_DATAW data;

  EXPECT_EQ(0U, SandboxFindFirstFile(search_path, &data, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  base::string16 first_found = data.cFileName;
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(file_path_1.BaseName().value(),
                                               data.cFileName))
      << "Returned file name doesn't match, expected "
      << file_path_1.BaseName().value() << " and got " << data.cFileName;

  EXPECT_EQ(static_cast<uint32_t>(ERROR_NO_MORE_FILES),
            SandboxFindNextFile(handle, &data));

  EXPECT_EQ(0U, SandboxFindClose(handle));
}

TEST(ScannerSandboxInterface, FindClose_Invalid) {
  EXPECT_EQ(static_cast<uint32_t>(ERROR_INVALID_HANDLE),
            SandboxFindClose(INVALID_HANDLE_VALUE));
}

TEST(ScannerSandboxInterface, GetFileAttributes_Valid) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"file.txt");
  ASSERT_TRUE(chrome_cleaner::CreateEmptyFile(file_path));

  uint32_t attributes = 0;
  int32_t result = SandboxGetFileAttributes(file_path, &attributes);
  EXPECT_NE(INVALID_FILE_ATTRIBUTES, attributes);
  EXPECT_EQ(ERROR_SUCCESS, result);
}

TEST(ScannerSandboxInterface, GetFileAttributes_MissingFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"missing_file.txt");

  uint32_t attributes = 0;
  int32_t result = SandboxGetFileAttributes(file_path, &attributes);
  EXPECT_EQ(INVALID_FILE_ATTRIBUTES, attributes);
  EXPECT_EQ(ERROR_FILE_NOT_FOUND, result);
}

TEST(ScannerSandboxInterface, GetFileAttributes_EmptyPath) {
  uint32_t attributes = 0;
  uint32_t result = SandboxGetFileAttributes(base::FilePath(), &attributes);
  EXPECT_EQ(INVALID_FILE_ATTRIBUTES, attributes);
  EXPECT_EQ(SandboxErrorCode::INVALID_FILE_PATH, result);
}

TEST(ScannerSandboxInterface, GetFileAttributes_RelativePath) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_name(L"file.txt");
  ASSERT_TRUE(
      chrome_cleaner::CreateEmptyFile(temp.GetPath().Append(file_name)));
  ScopedCurrentDirectory directory_override(temp.GetPath());

  uint32_t attributes = 0;
  uint32_t result = SandboxGetFileAttributes(file_name, &attributes);
  EXPECT_EQ(INVALID_FILE_ATTRIBUTES, attributes);
  EXPECT_EQ(SandboxErrorCode::INVALID_FILE_PATH, result);
}

TEST(ScannerSandboxInterface, GetFileAttributes_Wow64Disabled) {
  static constexpr wchar_t kTestFile[] = L"temp_file.txt";

  ScopedTempDirNoWow64 temp_dir;
  ASSERT_TRUE(temp_dir.CreateEmptyFileInUniqueSystem32TempDir(kTestFile));
  base::FilePath base_path = temp_dir.GetPath().BaseName();

  // Make sure the file can be found in the system32 directory.
  base::FilePath system_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &system_path));

  {
    base::FilePath search_path =
        system_path.Append(base_path).Append(kTestFile);
    SCOPED_TRACE(::testing::Message() << "search_path = " << search_path);
    uint32_t attributes;
    int32_t result = SandboxGetFileAttributes(search_path, &attributes);
    EXPECT_EQ(ERROR_SUCCESS, result);
  }

  // Make sure the file is NOT found in the redirected directory. Skip this
  // test on 32-bit Windows because the redirected path will be empty as Wow64
  // redirection is not supported.
  if (chrome_cleaner::IsX64Architecture()) {
    base::FilePath redirected_path = GetWow64RedirectedSystemPath();
    ASSERT_FALSE(redirected_path.empty());
    ASSERT_NE(redirected_path, system_path);
    base::FilePath search_path =
        redirected_path.Append(base_path).Append(kTestFile);
    SCOPED_TRACE(::testing::Message() << "search_path = " << search_path);

    uint32_t attributes;
    int32_t result = SandboxGetFileAttributes(search_path, &attributes);
    EXPECT_EQ(ERROR_PATH_NOT_FOUND, result);
    EXPECT_EQ(INVALID_FILE_ATTRIBUTES, attributes);
  }
}

TEST(ScannerSandboxInterface, GetFileAttributes_WithNetworkedFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"file.txt");
  ASSERT_TRUE(CreateNetworkedFile(file_path));

  uint32_t attributes = 0;
  int32_t result = SandboxGetFileAttributes(file_path, &attributes);
  EXPECT_EQ(INVALID_FILE_ATTRIBUTES, attributes);
  EXPECT_EQ(ERROR_FILE_NOT_FOUND, result);
}

TEST(ScannerSandboxInterface, GetKnownFolderPath) {
  EXPECT_FALSE(SandboxGetKnownFolderPath(KnownFolder::kWindows, nullptr));

  base::FilePath folder_path;
  EXPECT_FALSE(
      SandboxGetKnownFolderPath(static_cast<KnownFolder>(-1), &folder_path));

  EXPECT_TRUE(SandboxGetKnownFolderPath(KnownFolder::kWindows, &folder_path));
  base::FilePath windows_dir;
  base::PathService::Get(base::DIR_WINDOWS, &windows_dir);
  EXPECT_EQ(base::ToLowerASCII(windows_dir.value()),
            base::ToLowerASCII(folder_path.value()));

  EXPECT_TRUE(
      SandboxGetKnownFolderPath(KnownFolder::kProgramFiles, &folder_path));
  base::FilePath program_files_dir;
  base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir);
  EXPECT_EQ(base::ToLowerASCII(program_files_dir.value()),
            base::ToLowerASCII(folder_path.value()));

  EXPECT_TRUE(SandboxGetKnownFolderPath(KnownFolder::kAppData, &folder_path));
  base::FilePath appdata_dir;
  base::PathService::Get(base::DIR_APP_DATA, &appdata_dir);
  EXPECT_EQ(base::ToLowerASCII(appdata_dir.value()),
            base::ToLowerASCII(folder_path.value()));
}

TEST(ScannerSandboxInterface, GetProcesses) {
  ASSERT_FALSE(SandboxGetProcesses(nullptr));

  std::vector<base::ProcessId> processes;
  ASSERT_TRUE(SandboxGetProcesses(&processes));

  base::ProcessId current_pid = ::GetCurrentProcessId();
  EXPECT_EQ(1, std::count(processes.begin(), processes.end(), current_pid));
  EXPECT_LT(1UL, processes.size());
}

TEST(ScannerSandboxInterface, GetTasks) {
  ASSERT_FALSE(SandboxGetTasks(nullptr));

  chrome_cleaner::TestTaskScheduler task_scheduler;
  chrome_cleaner::TaskScheduler::TaskInfo created_task_info;
  ASSERT_TRUE(RegisterTestTask(&task_scheduler, &created_task_info));

  std::vector<chrome_cleaner::TaskScheduler::TaskInfo> retrieved_tasks;
  EXPECT_TRUE(SandboxGetTasks(&retrieved_tasks));

  int matching_tasks = 0;
  for (const auto& retrieved_task : retrieved_tasks) {
    if (retrieved_task.name == created_task_info.name &&
        retrieved_task.description == created_task_info.description &&
        retrieved_task.exec_actions.size() ==
            created_task_info.exec_actions.size()) {
      bool all_actions_match = true;
      for (size_t i = 0; i < retrieved_task.exec_actions.size(); ++i) {
        if (retrieved_task.exec_actions[i] !=
            created_task_info.exec_actions[i]) {
          all_actions_match = false;
          break;
        }
      }
      if (all_actions_match)
        ++matching_tasks;
    }
  }
  EXPECT_EQ(1, matching_tasks);
}

TEST(ScannerSandboxInterface, GetProcessImagePath_Self) {
  ASSERT_FALSE(SandboxGetProcessImagePath(::GetCurrentProcessId(), nullptr));

  base::FilePath image_path;
  ASSERT_TRUE(SandboxGetProcessImagePath(::GetCurrentProcessId(), &image_path));

  const base::FilePath exe_path =
      chrome_cleaner::PreFetchedPaths::GetInstance()->GetExecutablePath();
  EXPECT_EQ(base::ToLowerASCII(exe_path.value()),
            base::ToLowerASCII(image_path.value()));
}

TEST(ScannerSandboxInterface, GetProcessImagePath_InvalidPid) {
  base::FilePath image_path;
  // 0 is System Idle Process, and it's not possible to open it.
  ASSERT_FALSE(SandboxGetProcessImagePath(0, &image_path));
}

TEST(ScannerSandboxInterface, GetLoadedModules_Self) {
  ASSERT_FALSE(SandboxGetLoadedModules(::GetCurrentProcessId(), nullptr));

  std::set<base::string16> module_names;
  ASSERT_TRUE(SandboxGetLoadedModules(::GetCurrentProcessId(), &module_names));

  // Every process contains its executable as a module.
  const base::FilePath exe_path =
      chrome_cleaner::PreFetchedPaths::GetInstance()->GetExecutablePath();
  EXPECT_NE(module_names.end(), module_names.find(exe_path.value()));
  EXPECT_LT(1UL, module_names.size());
}

TEST(ScannerSandboxInterface, GetLoadedModules_InvalidPid) {
  std::set<base::string16> module_names;
  // 0 is System Idle Process, and it's not possible to open it.
  EXPECT_FALSE(SandboxGetLoadedModules(0, &module_names));
}

TEST(ScannerSandboxInterface, GetProcessCommandLine_Success) {
  base::CommandLine test_process_cmd(base::CommandLine::NO_PROGRAM);
  base::Process test_process =
      chrome_cleaner::LongRunningProcess(&test_process_cmd);
  ASSERT_TRUE(test_process.IsValid());

  base::string16 command_line;
  EXPECT_TRUE(SandboxGetProcessCommandLine(test_process.Pid(), &command_line));
  EXPECT_EQ(test_process_cmd.GetCommandLineString(), command_line);

  test_process.Terminate(/*exit_code=*/1, /*wait=*/false);
}

TEST(ScannerSandboxInterface, GetProcessCommandLine_AccessDenied) {
  base::CommandLine test_process_cmd(base::CommandLine::NO_PROGRAM);
  base::Process test_process =
      chrome_cleaner::LongRunningProcess(&test_process_cmd);
  ASSERT_TRUE(test_process.IsValid());

  {
    // Set up a ScopedProcessProtector that removes only some access rights.
    chrome_cleaner::ScopedProcessProtector process_protector(
        test_process.Pid(), PROCESS_QUERY_INFORMATION);
    base::string16 command_line;
    EXPECT_FALSE(
        SandboxGetProcessCommandLine(test_process.Pid(), &command_line));
  }

  test_process.Terminate(/*exit_code=*/1, /*wait=*/false);
}

TEST(ScannerSandboxInterface, GetProcessCommandLine_InvalidInput) {
  EXPECT_FALSE(SandboxGetProcessCommandLine(::GetCurrentProcessId(), nullptr));

  base::string16 command_line;
  // 0 is System Idle Process, and it's not possible to open it.
  EXPECT_FALSE(SandboxGetProcessCommandLine(0, &command_line));
}

TEST(ScannerSandboxInterface, GetUserInfoFromSID) {
  sandbox::Sid sid(WinLocalSid);
  EXPECT_FALSE(
      SandboxGetUserInfoFromSID(static_cast<SID*>(sid.GetPSID()), nullptr));

  chrome_cleaner::mojom::UserInformation user_info;
  EXPECT_TRUE(
      SandboxGetUserInfoFromSID(static_cast<SID*>(sid.GetPSID()), &user_info));
  EXPECT_EQ(L"LOCAL", user_info.name);
  EXPECT_EQ(L"", user_info.domain);
  EXPECT_EQ(static_cast<uint32_t>(SidTypeWellKnownGroup),
            user_info.account_type);

  sid = sandbox::Sid(WinSelfSid);
  EXPECT_TRUE(
      SandboxGetUserInfoFromSID(static_cast<SID*>(sid.GetPSID()), &user_info));
  EXPECT_EQ(L"SELF", user_info.name);
  EXPECT_EQ(L"NT AUTHORITY", user_info.domain);
  EXPECT_EQ(static_cast<uint32_t>(SidTypeWellKnownGroup),
            user_info.account_type);
}

class ScannerSandboxInterface_OpenReadOnlyFile : public ::testing::Test {
 public:
  void TearDown() override {
    chrome_cleaner::Settings::SetInstanceForTesting(nullptr);
  }
};

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, BasicFile) {
  const wchar_t file_name[] = L"temp_file.txt";

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(file_name);

  ASSERT_TRUE(
      chrome_cleaner::CreateFileInFolder(file_path.DirName(), file_name));

  base::win::ScopedHandle handle(
      SandboxOpenReadOnlyFile(file_path, FILE_ATTRIBUTE_NORMAL));
  EXPECT_TRUE(handle.IsValid());

  handle = SandboxOpenReadOnlyFile(file_path,
                                   /*dwFlagsAndAttributes=*/0);
  EXPECT_TRUE(handle.IsValid());

  // Make sure the file cannot be opened using a relative path.
  ScopedCurrentDirectory directory_override(temp.GetPath());
  base::win::ScopedHandle relative_handle =
      SandboxOpenReadOnlyFile(base::FilePath(file_name), FILE_ATTRIBUTE_NORMAL);
  EXPECT_FALSE(relative_handle.IsValid());

  // Make sure that the file path requires a drive letter.
  base::FilePath native_path = GetNativePath(file_path.value());
  base::win::ScopedHandle native_handle =
      SandboxOpenReadOnlyFile(native_path, FILE_ATTRIBUTE_NORMAL);
  EXPECT_FALSE(native_handle.IsValid()) << native_path;

  base::FilePath universal_path = GetUniversalPath(file_path.value());
  base::win::ScopedHandle universal_handle =
      SandboxOpenReadOnlyFile(universal_path, FILE_ATTRIBUTE_NORMAL);
  EXPECT_FALSE(universal_handle.IsValid()) << universal_path;

  // Make sure the file can be opened using a path with trailing whitespaces.
  const base::string16 path_with_space = file_path.value() + L" ";
  handle = SandboxOpenReadOnlyFile(base::FilePath(path_with_space),
                                   FILE_ATTRIBUTE_NORMAL);
  EXPECT_TRUE(handle.IsValid());

  // Make sure quotes aren't interpreted. The same path might be passed to
  // SandboxDeleteFile, which doesn't interpret quotes.
  const base::FilePath quoted_path(L"\"" + file_path.value() + L"\"");
  handle = SandboxOpenReadOnlyFile(quoted_path, FILE_ATTRIBUTE_NORMAL);
  EXPECT_FALSE(handle.IsValid())
      << "SandboxOpenReadOnlyFile is interpreting quotes around path names; "
         "this will cause problems if the same path is passed to DeleteFile "
         "(see CleanerSandboxInterfaceDeleteFileTest.QuotedPath)";

  const base::FilePath partly_quoted_path =
      temp.GetPath().Append(L"\"temp_file.exe\"");
  handle = SandboxOpenReadOnlyFile(partly_quoted_path, FILE_ATTRIBUTE_NORMAL);
  EXPECT_FALSE(handle.IsValid())
      << "SandboxOpenReadOnlyFile is interpreting quotes around path "
         "components; this will cause problems if the same path is passed to "
         "DeleteFile (see "
         "CleanerSandboxInterfaceDeleteFileTest.QuotedFilename)";
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, NoFile) {
  const base::FilePath file_path(L"C:/fake/path/that/I/made/up.txt");
  base::win::ScopedHandle handle(
      SandboxOpenReadOnlyFile(file_path, FILE_ATTRIBUTE_NORMAL));
  EXPECT_FALSE(handle.IsValid());
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, NoFileName) {
  base::win::ScopedHandle handle(
      SandboxOpenReadOnlyFile(base::FilePath(), FILE_ATTRIBUTE_NORMAL));
  EXPECT_FALSE(handle.IsValid());
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, ValidDwFlagsAndAttributes) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.txt");

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  base::win::ScopedHandle handle(SandboxOpenReadOnlyFile(
      file_path, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING));
  EXPECT_TRUE(handle.IsValid());

  handle = SandboxOpenReadOnlyFile(
      file_path, FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS);
  EXPECT_TRUE(handle.IsValid());

  handle = SandboxOpenReadOnlyFile(
      file_path, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN |
                     FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_OPEN_REPARSE_POINT);
  EXPECT_TRUE(handle.IsValid());
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, InvalidDwFlagsAndAttributes) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.txt");

  ASSERT_TRUE(chrome_cleaner::CreateFileInFolder(
      file_path.DirName(), file_path.BaseName().value().c_str()));

  base::win::ScopedHandle handle(
      SandboxOpenReadOnlyFile(file_path, FILE_FLAG_WRITE_THROUGH));
  EXPECT_FALSE(handle.IsValid()) << "file_path = " << file_path.value();

  handle = SandboxOpenReadOnlyFile(
      file_path, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
  EXPECT_FALSE(handle.IsValid()) << "file_path = " << file_path.value();
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, Wow64Disabled) {
  static constexpr wchar_t kTestFile[] = L"temp_file.txt";

  ScopedTempDirNoWow64 temp_dir;
  ASSERT_TRUE(temp_dir.CreateEmptyFileInUniqueSystem32TempDir(kTestFile));
  base::FilePath base_path = temp_dir.GetPath().BaseName();

  // Make sure the file can be found in the system32 directory.
  base::FilePath system_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &system_path));
  base::FilePath file_path = system_path.Append(base_path).Append(kTestFile);
  base::win::ScopedHandle handle(
      SandboxOpenReadOnlyFile(file_path,
                              /*dwFlagsAndAttributes=*/0));
  EXPECT_TRUE(handle.IsValid());

  // If this configuration uses Wow64 redirection, make sure the file is NOT
  // found in the redirected directory. Skip this test on 32-bit Windows
  // because the redirected path will be empty as Wow64 redirection is not
  // supported.
  if (chrome_cleaner::IsX64Architecture()) {
    base::FilePath redirected_path = GetWow64RedirectedSystemPath();
    ASSERT_FALSE(redirected_path.empty());
    ASSERT_NE(redirected_path, system_path);
    file_path = redirected_path.Append(base_path).Append(kTestFile);

    handle = SandboxOpenReadOnlyFile(file_path,
                                     /*dwFlagsAndAttributes=*/0);
    EXPECT_FALSE(handle.IsValid()) << "file_path = " << file_path.value();
  }
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, NetworkedFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"temp_file.txt");

  ASSERT_TRUE(CreateNetworkedFile(file_path));

  base::win::ScopedHandle handle(
      SandboxOpenReadOnlyFile(file_path, FILE_ATTRIBUTE_NORMAL));
  EXPECT_FALSE(handle.IsValid());

  handle = SandboxOpenReadOnlyFile(file_path,
                                   /*dwFlagsAndAttributes=*/0);
  EXPECT_FALSE(handle.IsValid());
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, BelowFileSizeLimit) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"file.txt");
  const char kFileContent[] = "file content";
  chrome_cleaner::CreateFileWithContent(file_path, kFileContent,
                                        sizeof(kFileContent));

  chrome_cleaner::MockSettings mock_settings;
  EXPECT_CALL(mock_settings, open_file_size_limit())
      .WillOnce(testing::Return(1024));
  chrome_cleaner::Settings::SetInstanceForTesting(&mock_settings);

  base::win::ScopedHandle handle =
      SandboxOpenReadOnlyFile(file_path, FILE_ATTRIBUTE_NORMAL);
  EXPECT_TRUE(handle.IsValid());
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyFile, AboveFileSizeLimit) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath file_path = temp.GetPath().Append(L"file.txt");
  const char kFileContent[] = "file content";
  chrome_cleaner::CreateFileWithContent(file_path, kFileContent,
                                        sizeof(kFileContent));

  chrome_cleaner::MockSettings mock_settings;
  EXPECT_CALL(mock_settings, open_file_size_limit())
      .WillOnce(testing::Return(4));
  chrome_cleaner::Settings::SetInstanceForTesting(&mock_settings);

  base::win::ScopedHandle handle =
      SandboxOpenReadOnlyFile(file_path, FILE_ATTRIBUTE_NORMAL);
  EXPECT_FALSE(handle.IsValid());
}

class ScannerSandboxInterface_OpenReadOnlyRegistry : public ::testing::Test {
 public:
  void SetUp() override {
    override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    base::win::RegKey registry_key;
    key_value_name_ = L"dummy";
    ASSERT_EQ(ERROR_SUCCESS,
              registry_key.Create(HKEY_CURRENT_USER, key_value_name_.c_str(),
                                  KEY_ALL_ACCESS));
  }

  bool ValidKeyValue(HKEY key) {
    return key != static_cast<HKEY>(INVALID_HANDLE_VALUE);
  }

 protected:
  base::string16 key_value_name_;
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_F(ScannerSandboxInterface_OpenReadOnlyRegistry, ValidPath) {
  HKEY handle;
  EXPECT_EQ(0U, SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, key_value_name_,
                                            KEY_READ, &handle));
  EXPECT_TRUE(ValidKeyValue(handle));
  ::RegCloseKey(handle);
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyRegistry, DwAccess) {
  HKEY handle;
  EXPECT_EQ(0U,
            SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, key_value_name_,
                                        KEY_READ | KEY_WOW64_32KEY, &handle));
  EXPECT_TRUE(ValidKeyValue(handle));
  ::RegCloseKey(handle);

  EXPECT_EQ(0U,
            SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, key_value_name_,
                                        KEY_READ | KEY_WOW64_64KEY, &handle));
  EXPECT_TRUE(ValidKeyValue(handle));
  ::RegCloseKey(handle);

  EXPECT_EQ(0U, SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, key_value_name_,
                                            KEY_EXECUTE, &handle));
  EXPECT_TRUE(ValidKeyValue(handle));
  ::RegCloseKey(handle);

  EXPECT_EQ(0U, SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, key_value_name_,
                                            KEY_QUERY_VALUE | KEY_WOW64_64KEY,
                                            &handle));
  EXPECT_TRUE(ValidKeyValue(handle));
  ::RegCloseKey(handle);
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyRegistry, InvalidResultHandle) {
  EXPECT_EQ(SandboxErrorCode::NULL_OUTPUT_HANDLE,
            SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, key_value_name_,
                                        KEY_READ, nullptr));
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyRegistry, NonexistantPath) {
  const base::string16 fake_key = L"fake_key_name";

  HKEY handle = nullptr;
  EXPECT_EQ(
      static_cast<uint32_t>(ERROR_FILE_NOT_FOUND),
      SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, fake_key, 0, &handle));
  EXPECT_FALSE(ValidKeyValue(handle));
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyRegistry, InvalidDwAccess) {
  HKEY handle = nullptr;
  EXPECT_EQ(SandboxErrorCode::INVALID_DW_ACCESS,
            SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, key_value_name_,
                                        KEY_READ | KEY_SET_VALUE, &handle));
  EXPECT_FALSE(ValidKeyValue(handle));

  EXPECT_EQ(SandboxErrorCode::INVALID_DW_ACCESS,
            SandboxOpenReadOnlyRegistry(
                HKEY_CURRENT_USER, key_value_name_,
                KEY_READ | KEY_SET_VALUE | KEY_WOW64_32KEY, &handle));
  EXPECT_FALSE(ValidKeyValue(handle));
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyRegistry, NullRootKey) {
  HKEY handle = nullptr;
  EXPECT_EQ(SandboxErrorCode::NULL_ROOT_KEY,
            SandboxOpenReadOnlyRegistry(nullptr, key_value_name_, 0, &handle));
  EXPECT_FALSE(ValidKeyValue(handle));
}

TEST_F(ScannerSandboxInterface_OpenReadOnlyRegistry, EmptySubKey) {
  const base::string16 empty_key;
  HKEY handle = nullptr;
  EXPECT_EQ(0U, SandboxOpenReadOnlyRegistry(HKEY_CURRENT_USER, empty_key, 0,
                                            &handle));
  EXPECT_TRUE(ValidKeyValue(handle));
  ::RegCloseKey(handle);
}

class ScannerSandboxInterface_NtOpenReadOnlyRegistry : public ::testing::Test {
 public:
  void SetUp() override {
    base::string16 hklm_path_string = L"\\REGISTRY\\MACHINE\\";
    hklm_path_ = StringWithTrailingNull(hklm_path_string);

    base::string16 relative_path_string = temp_registry_key_.Path();
    relative_path_ = StringWithTrailingNull(relative_path_string);

    base::string16 fully_qualified_path_string =
        temp_registry_key_.FullyQualifiedPath();
    fully_qualified_path_ = StringWithTrailingNull(fully_qualified_path_string);
  }

 protected:
  String16EmbeddedNulls hklm_path_;
  String16EmbeddedNulls relative_path_;
  String16EmbeddedNulls fully_qualified_path_;
  chrome_cleaner_sandbox::ScopedTempRegistryKey temp_registry_key_;
};

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, FullyQualifiedSubKey) {
  HANDLE handle = INVALID_HANDLE_VALUE;

  EXPECT_EQ(0U, SandboxNtOpenReadOnlyRegistry(nullptr, fully_qualified_path_,
                                              KEY_READ, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  EXPECT_TRUE(::CloseHandle(handle));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry,
       NonNullRootAndFullyQualifiedSubKey) {
  HANDLE hklm_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(0U, SandboxNtOpenReadOnlyRegistry(nullptr, hklm_path_, KEY_READ,
                                              &hklm_handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, hklm_handle);

  HANDLE handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(static_cast<uint32_t>(STATUS_OBJECT_PATH_SYNTAX_BAD),
            SandboxNtOpenReadOnlyRegistry(hklm_handle, fully_qualified_path_,
                                          KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
  EXPECT_TRUE(::CloseHandle(hklm_handle));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry,
       NullRootAndRelativeSubKey) {
  HANDLE handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(SandboxErrorCode::NULL_ROOT_AND_RELATIVE_SUB_KEY,
            SandboxNtOpenReadOnlyRegistry(nullptr, relative_path_, KEY_READ,
                                          &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, ValidPathWithRootKey) {
  HANDLE hklm_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(STATUS_SUCCESS,
            NativeOpenKey(NULL, hklm_path_, KEY_ALL_ACCESS, &hklm_handle));

  HANDLE handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(0U, SandboxNtOpenReadOnlyRegistry(hklm_handle, relative_path_,
                                              KEY_READ, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);

  EXPECT_TRUE(::CloseHandle(handle));
  EXPECT_TRUE(::CloseHandle(hklm_handle));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry,
       NullRootAndEmbeddedNullsInSubKey) {
  std::vector<wchar_t> sub_key{L'n', L'u', L'l', L'\0', L'l', L'\0'};
  HANDLE sub_key_handle = INVALID_HANDLE_VALUE;
  ULONG disposition = 0;
  EXPECT_EQ(STATUS_SUCCESS, NativeCreateKey(temp_registry_key_.Get(), &sub_key,
                                            &sub_key_handle, &disposition));
  EXPECT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);

  // Build up a fully qualified path to the sub key that has nulls in it.
  std::vector<wchar_t> full_path(fully_qualified_path_.data());
  // Remove the last trailing null from full_path to allow appending more.
  full_path.pop_back();
  full_path.push_back(L'\\');
  full_path.insert(full_path.end(), sub_key.begin(), sub_key.end());

  HANDLE handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(0U,
            SandboxNtOpenReadOnlyRegistry(
                nullptr, String16EmbeddedNulls(full_path), KEY_READ, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  EXPECT_TRUE(::CloseHandle(handle));

  EXPECT_EQ(STATUS_SUCCESS, NativeDeleteKey(sub_key_handle));
  EXPECT_TRUE(::CloseHandle(sub_key_handle));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry,
       ValidRootAndEmbeddedNullsInSubKey) {
  std::vector<wchar_t> sub_key{L'n', L'\0', L'n', L'u', L'l', L'l', L'\0'};

  ULONG disposition = 0;
  HANDLE temp_key = INVALID_HANDLE_VALUE;
  EXPECT_EQ(STATUS_SUCCESS, NativeCreateKey(temp_registry_key_.Get(), &sub_key,
                                            &temp_key, &disposition));
  EXPECT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);

  HANDLE handle;
  EXPECT_EQ(0U, SandboxNtOpenReadOnlyRegistry(temp_registry_key_.Get(),
                                              String16EmbeddedNulls(sub_key),
                                              KEY_READ, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);

  EXPECT_TRUE(::CloseHandle(handle));
  EXPECT_EQ(STATUS_SUCCESS, NativeDeleteKey(temp_key));
  EXPECT_TRUE(::CloseHandle(temp_key));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, ValidRootWithNullChars) {
  std::vector<wchar_t> root_with_null{L'n', L'u', L'l', L'\0', L'l', L'\0'};
  ULONG disposition = 0;
  HANDLE root_with_null_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(STATUS_SUCCESS,
            NativeCreateKey(temp_registry_key_.Get(), &root_with_null,
                            &root_with_null_handle, &disposition));
  EXPECT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);

  HANDLE read_only_root_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(0U,
            SandboxNtOpenReadOnlyRegistry(temp_registry_key_.Get(),
                                          String16EmbeddedNulls(root_with_null),
                                          KEY_READ, &read_only_root_handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, read_only_root_handle);
  EXPECT_TRUE(::CloseHandle(read_only_root_handle));

  std::vector<wchar_t> child_key{L'c', L'h', L'i', L'l', L'\0', L'd', L'\0'};

  disposition = 0;
  HANDLE child_key_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(STATUS_SUCCESS, NativeCreateKey(root_with_null_handle, &child_key,
                                            &child_key_handle, &disposition));
  EXPECT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);

  HANDLE read_only_child_key_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(0U, SandboxNtOpenReadOnlyRegistry(
                    root_with_null_handle, String16EmbeddedNulls(child_key),
                    KEY_READ, &read_only_child_key_handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, read_only_child_key_handle);
  EXPECT_TRUE(::CloseHandle(read_only_child_key_handle));

  // Clean up. Carefully.
  EXPECT_EQ(STATUS_SUCCESS, NativeDeleteKey(child_key_handle));
  EXPECT_TRUE(::CloseHandle(child_key_handle));
  EXPECT_EQ(STATUS_SUCCESS, NativeDeleteKey(root_with_null_handle));
  EXPECT_TRUE(::CloseHandle(root_with_null_handle));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry,
       InvalidPathWithNullRootKey) {
  base::string16 fake_path = L"\\REGISTRY\\MACHINE\\fake\\path";

  HANDLE handle;
  EXPECT_EQ(static_cast<uint32_t>(STATUS_OBJECT_NAME_NOT_FOUND),
            SandboxNtOpenReadOnlyRegistry(
                nullptr, StringWithTrailingNull(fake_path), KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, InvalidHandle) {
  EXPECT_EQ(
      SandboxErrorCode::NULL_OUTPUT_HANDLE,
      SandboxNtOpenReadOnlyRegistry(nullptr, hklm_path_, KEY_READ, nullptr));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, InvalidRootKey) {
  HANDLE handle;
  EXPECT_EQ(SandboxErrorCode::INVALID_KEY,
            SandboxNtOpenReadOnlyRegistry(INVALID_HANDLE_VALUE, relative_path_,
                                          KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, InvalidSubkey) {
  HANDLE handle;
  EXPECT_EQ(SandboxErrorCode::NULL_SUB_KEY,
            SandboxNtOpenReadOnlyRegistry(
                nullptr, String16EmbeddedNulls(nullptr), KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);

  base::string16 very_long_name =
      base::string16(fully_qualified_path_.CastAsWCharArray()) +
      base::string16(kMaxRegistryParamLength, L'a');
  EXPECT_EQ(
      SandboxErrorCode::INVALID_SUBKEY_STRING,
      SandboxNtOpenReadOnlyRegistry(
          nullptr, StringWithTrailingNull(very_long_name), KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);

  // Use a valid key name to be sure that errors reported are due to the length
  // parameter, not the key name.
  EXPECT_EQ(
      SandboxErrorCode::NULL_SUB_KEY,
      SandboxNtOpenReadOnlyRegistry(
          nullptr, String16EmbeddedNulls(hklm_path_.CastAsWCharArray(), 0),
          KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);

  EXPECT_EQ(SandboxErrorCode::INVALID_SUBKEY_STRING,
            SandboxNtOpenReadOnlyRegistry(
                nullptr,
                String16EmbeddedNulls(hklm_path_.CastAsWCharArray(),
                                      hklm_path_.size() - 1),
                KEY_READ, &handle))
      << "sub_key should be invalid when missing null terminator";
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, ValidDwAccess) {
  HANDLE handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(0U, SandboxNtOpenReadOnlyRegistry(nullptr, fully_qualified_path_,
                                              STANDARD_RIGHTS_READ, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  EXPECT_TRUE(::CloseHandle(handle));

  EXPECT_EQ(0U, SandboxNtOpenReadOnlyRegistry(nullptr, fully_qualified_path_,
                                              KEY_EXECUTE, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  EXPECT_TRUE(::CloseHandle(handle));

  EXPECT_EQ(0U,
            SandboxNtOpenReadOnlyRegistry(nullptr, fully_qualified_path_,
                                          KEY_READ | KEY_WOW64_32KEY, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  EXPECT_TRUE(::CloseHandle(handle));

  EXPECT_EQ(0U,
            SandboxNtOpenReadOnlyRegistry(nullptr, fully_qualified_path_,
                                          KEY_READ | KEY_WOW64_64KEY, &handle));
  EXPECT_NE(INVALID_HANDLE_VALUE, handle);
  EXPECT_TRUE(::CloseHandle(handle));
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, InvalidDwAccess) {
  HANDLE handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(SandboxErrorCode::INVALID_DW_ACCESS,
            SandboxNtOpenReadOnlyRegistry(nullptr, fully_qualified_path_,
                                          KEY_ALL_ACCESS, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);

  EXPECT_EQ(SandboxErrorCode::INVALID_DW_ACCESS,
            SandboxNtOpenReadOnlyRegistry(nullptr, fully_qualified_path_,
                                          KEY_SET_VALUE, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

TEST_F(ScannerSandboxInterface_NtOpenReadOnlyRegistry, NonRegistryPath) {
  const base::string16 direct_path = L"\\DosDevice\\C:";
  const base::string16 tricky_path =
      L"\\Registry\\Machine\\..\\..\\DosDevice\\C:";

  HANDLE handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(
      SandboxErrorCode::INVALID_SUBKEY_STRING,
      SandboxNtOpenReadOnlyRegistry(
          nullptr, StringWithTrailingNull(direct_path), KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);

  // We depend on the OS to deny access to non-registry keys, so the exact
  // error code this returns is obscure. Just as long as it's not success.
  EXPECT_NE(
      static_cast<uint32_t>(STATUS_SUCCESS),
      SandboxNtOpenReadOnlyRegistry(
          nullptr, StringWithTrailingNull(tricky_path), KEY_READ, &handle));
  EXPECT_EQ(INVALID_HANDLE_VALUE, handle);
}

}  // namespace chrome_cleaner_sandbox
