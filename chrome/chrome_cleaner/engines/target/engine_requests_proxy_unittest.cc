// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_requests_proxy.h"

#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/chrome_cleaner/engines/common/registry_util.h"
#include "chrome/chrome_cleaner/engines/target/sandboxed_test_helpers.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"
#include "chrome/chrome_cleaner/strings/string_test_helpers.h"
#include "chrome/chrome_cleaner/test/test_native_reg_util.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "sandbox/win/src/sid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

// Temp keys to create under HKLM.
constexpr char kTempKeyPathSwitch[] = "temp-key-path";
constexpr char kTempKeyFullPathSwitch[] = "temp-key-full-path";
constexpr wchar_t kKeyWithNulls[] = L"fake0key0with0nulls";

// Switch with a path to the windows directory.
constexpr char kWindowsDirectorySwitch[] = "windows-directory";

class TestChildProcess : public SandboxChildProcess {
 public:
  explicit TestChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : SandboxChildProcess(std::move(mojo_task_runner)) {}

  bool Initialize() {
    LowerToken();

    windows_directory_ =
        command_line().GetSwitchValuePath(kWindowsDirectorySwitch);
    if (windows_directory_.empty()) {
      LOG(ERROR) << "Initialize failed: Missing " << kWindowsDirectorySwitch
                 << " switch";
      return false;
    }

    temp_key_path_ = command_line().GetSwitchValueNative(kTempKeyPathSwitch);
    if (temp_key_path_.empty()) {
      LOG(ERROR) << "Initialize failed: Missing " << kTempKeyPathSwitch
                 << " switch";
      return false;
    }

    temp_key_full_path_ =
        command_line().GetSwitchValueNative(kTempKeyFullPathSwitch);
    if (temp_key_full_path_.empty()) {
      LOG(ERROR) << "Initialize failed: Missing " << kTempKeyFullPathSwitch
                 << " switch";
      return false;
    }

    return true;
  }

  base::FilePath windows_directory() const { return windows_directory_; }

  base::string16 temp_key_path() const { return temp_key_path_; }

  base::string16 temp_key_full_path() const { return temp_key_full_path_; }

 private:
  ~TestChildProcess() override = default;

  base::FilePath windows_directory_;
  base::string16 temp_key_path_;
  base::string16 temp_key_full_path_;
};

scoped_refptr<TestChildProcess> SetupSandboxedChildProcess() {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  auto child_process = base::MakeRefCounted<TestChildProcess>(mojo_task_runner);
  if (!child_process->Initialize())
    return base::MakeRefCounted<TestChildProcess>(nullptr);
  return child_process;
}

MULTIPROCESS_TEST_MAIN(GetFileAttributesTest) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  uint32_t attributes;
  EXPECT_EQ(INVALID_FILE_PATH,
            proxy->GetFileAttributes(base::FilePath(), &attributes));

  EXPECT_EQ(NULL_DATA_HANDLE, proxy->GetFileAttributes(
                                  child_process->windows_directory(), nullptr));

  EXPECT_EQ(uint32_t{ERROR_SUCCESS},
            proxy->GetFileAttributes(child_process->windows_directory(),
                                     &attributes));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetFileAttributesNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  uint32_t attributes;
  EXPECT_EQ(INTERNAL_ERROR,
            proxy->GetFileAttributes(child_process->windows_directory(),
                                     &attributes));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetKnownFolderPath) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(
      proxy->GetKnownFolderPath(mojom::KnownFolder::kWindows, nullptr));

  base::FilePath folder_path;
  if (!proxy->GetKnownFolderPath(mojom::KnownFolder::kWindows, &folder_path)) {
    LOG(ERROR) << "Failed to call GetKnownFolderPathCallback";
    return 1;
  }

  if (!base::EqualsCaseInsensitiveASCII(
          child_process->windows_directory().value(), folder_path.value())) {
    LOG(ERROR) << "Retrieved known folder path was " << folder_path
               << " expected " << child_process->windows_directory();
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetKnownFolderPathInvalidParam) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  base::FilePath folder_path;
  // This call should trigger mojo deserialization error, the broker will close
  // the pipe, which will cause the current process to die.
  proxy->GetKnownFolderPath(static_cast<mojom::KnownFolder>(-1), &folder_path);

  LOG(ERROR) << "Child process still alive after sending invalid enum value";
  return 1;
}

MULTIPROCESS_TEST_MAIN(GetKnownFolderPathNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  base::FilePath folder_path;
  EXPECT_FALSE(
      proxy->GetKnownFolderPath(mojom::KnownFolder::kWindows, &folder_path));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetProcesses) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetProcesses(nullptr));

  std::vector<base::ProcessId> processes;
  if (!proxy->GetProcesses(&processes)) {
    LOG(ERROR) << "Failed to call GetProcesses";
    return 1;
  }

  base::ProcessId current_pid = ::GetCurrentProcessId();
  if (std::count(processes.begin(), processes.end(), current_pid) != 1) {
    LOG(ERROR)
        << "Failed to find current process in list of returned processes";
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetProcessesNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetProcesses(nullptr));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetTasks) {
  // Enable COM and the TaskScheduler. In the broker process this is done in
  // test_main.cc, but we don't want to enable COM in the sandbox process
  // except in tests where it's actually used.
  base::win::ScopedCOMInitializer scoped_com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!TaskScheduler::Initialize()) {
    LOG(ERROR) << "TaskScheduler::Initialize() failed.";
    return 1;
  }

  // Create a test task.
  TaskScheduler* task_scheduler = TaskScheduler::CreateInstance();
  TaskScheduler::TaskInfo created_task;
  if (!RegisterTestTask(task_scheduler, &created_task)) {
    LOG(ERROR) << "Failed to create a test task";
    return 1;
  }

  // Ensure the test task is deleted when the test is finished.
  base::ScopedClosureRunner scoped_exit(base::BindOnce(
      base::IgnoreResult(&TaskScheduler::DeleteTask),
      base::Unretained(task_scheduler), created_task.name.c_str()));

  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetTasks(nullptr));

  std::vector<TaskScheduler::TaskInfo> tasks;
  if (!proxy->GetTasks(&tasks)) {
    LOG(ERROR) << "Failed to call GetTasks";
    return 1;
  }

  int matching_tasks = 0;
  for (const auto& task : tasks) {
    if (task.name == created_task.name &&
        task.description == created_task.description &&
        task.exec_actions.size() == created_task.exec_actions.size()) {
      bool all_actions_match = true;
      for (size_t i = 0; i < task.exec_actions.size(); ++i) {
        if (task.exec_actions[i].application_path !=
                created_task.exec_actions[i].application_path ||
            task.exec_actions[i].working_dir !=
                created_task.exec_actions[i].working_dir ||
            task.exec_actions[i].arguments !=
                created_task.exec_actions[i].arguments) {
          all_actions_match = false;
          break;
        }
      }
      if (all_actions_match)
        ++matching_tasks;
    }
  }

  if (matching_tasks != 1) {
    LOG(ERROR) << "Didn't get the expected number of matching tasks. Expected "
                  "1 and got "
               << matching_tasks;
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetTasksNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetTasks(nullptr));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetProcessImagePath) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetProcessImagePath(::GetCurrentProcessId(), nullptr));

  base::FilePath image_path;
  if (!proxy->GetProcessImagePath(::GetCurrentProcessId(), &image_path)) {
    LOG(ERROR) << "Failed to get current image path";
    return 1;
  }

  const base::FilePath exe_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();

  if (!base::EqualsCaseInsensitiveASCII(exe_path.value(), image_path.value())) {
    LOG(ERROR) << "Retrieved image path was " << image_path << " expected "
               << exe_path;
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetProcessImagePathNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetProcessImagePath(0, nullptr));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetLoadedModules) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetLoadedModules(::GetCurrentProcessId(), nullptr));

  std::vector<base::string16> module_names;
  if (!proxy->GetLoadedModules(::GetCurrentProcessId(), &module_names)) {
    LOG(ERROR) << "Failed to get loaded modules for current process";
    return 1;
  }

  // Every process contains its executable as a module.
  const base::FilePath exe_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();
  if (std::count(module_names.begin(), module_names.end(), exe_path.value()) !=
      1) {
    LOG(ERROR) << "Failed to find executable in own process";
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetLoadedModulesNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetLoadedModules(0, nullptr));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetProcessCommandLine) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetProcessCommandLine(::GetCurrentProcessId(), nullptr));

  base::string16 retrieved_cmd;
  if (!proxy->GetProcessCommandLine(::GetCurrentProcessId(), &retrieved_cmd)) {
    LOG(ERROR) << "Failed to get command line for the current process";
    return 1;
  }

  const base::CommandLine* current_cmd = base::CommandLine::ForCurrentProcess();
  base::CommandLine cmd = base::CommandLine::FromString(retrieved_cmd);
  EXPECT_EQ(current_cmd->GetProgram(), cmd.GetProgram());
  EXPECT_EQ(current_cmd->GetSwitches(), cmd.GetSwitches());
  EXPECT_EQ(current_cmd->GetArgs(), cmd.GetArgs());

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetProcessCommandLineNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());
  base::string16 cmd;
  EXPECT_FALSE(proxy->GetProcessCommandLine(::GetCurrentProcessId(), &cmd));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetUserInfoFromSID) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  EXPECT_FALSE(proxy->GetUserInfoFromSID(nullptr, nullptr));

  mojom::UserInformation user_info;
  EXPECT_FALSE(proxy->GetUserInfoFromSID(nullptr, &user_info));

  sandbox::Sid sid(WinSelfSid);
  EXPECT_FALSE(
      proxy->GetUserInfoFromSID(static_cast<SID*>(sid.GetPSID()), nullptr));
  if (!proxy->GetUserInfoFromSID(static_cast<SID*>(sid.GetPSID()),
                                 &user_info)) {
    LOG(ERROR) << "Failed to get user infomation";
    return 1;
  }

  EXPECT_EQ(L"SELF", user_info.name);
  EXPECT_EQ(L"NT AUTHORITY", user_info.domain);
  EXPECT_EQ(SidTypeWellKnownGroup,
            static_cast<_SID_NAME_USE>(user_info.account_type));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(GetUserInfoFromSIDNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;
  child_process->UnbindRequestsRemotes();

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  sandbox::Sid sid(WinLocalSid);
  mojom::UserInformation user_info;
  EXPECT_FALSE(
      proxy->GetUserInfoFromSID(static_cast<SID*>(sid.GetPSID()), &user_info));
  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(OpenReadOnlyRegistry) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  // TODO(joenotcharles): Test with all predefined keys and combinations of
  // WOW64 flags.
  const base::string16 fake_key_name = L"fake/key/I/just/made";
  HANDLE reg_handle;
  uint32_t result = proxy->OpenReadOnlyRegistry(
      HKEY_LOCAL_MACHINE, fake_key_name, KEY_READ, &reg_handle);
  if (reg_handle != INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Got a valid handle when trying to open a fake key";
    return 1;
  }
  if (result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << std::hex
               << "Got unexpected return code when opening a fake key. "
                  "Expected ERROR_FILE_NOT_FOUND(0x"
               << ERROR_FILE_NOT_FOUND << ") and got 0x" << result;
    return 1;
  }

  result = proxy->OpenReadOnlyRegistry(HKEY_LOCAL_MACHINE, base::string16(),
                                       KEY_READ, &reg_handle);
  if (reg_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << std::hex
               << "Failed to get a valid registry handle for "
                  "HKEY_LOCAL_MACHINE. Error code: 0x"
               << result;
    return 1;
  }

  result = proxy->OpenReadOnlyRegistry(HKEY_LOCAL_MACHINE,
                                       child_process->temp_key_path(), KEY_READ,
                                       &reg_handle);
  if (reg_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR)
        << std::hex
        << "Failed to get a valid registry handle for HKEY_CURRENT_USER\\"
        << child_process->temp_key_path() << ". Error code: 0x" << result;
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(OpenReadOnlyRegistryNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  HANDLE reg_handle;
  EXPECT_EQ(
      SandboxErrorCode::NULL_ROOT_KEY,
      proxy->OpenReadOnlyRegistry(nullptr, base::string16(), 0, &reg_handle));

  child_process->UnbindRequestsRemotes();

  EXPECT_EQ(
      SandboxErrorCode::INTERNAL_ERROR,
      proxy->OpenReadOnlyRegistry(nullptr, base::string16(), 0, &reg_handle));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(NtOpenReadOnlyRegistry) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  // Get an existing root key.
  HANDLE root_handle;
  uint32_t result = proxy->OpenReadOnlyRegistry(HKEY_LOCAL_MACHINE,
                                                child_process->temp_key_path(),
                                                KEY_READ, &root_handle);
  if (root_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR)
        << std::hex
        << "Failed to get a valid registry handle for HKEY_CURRENT_USER\\"
        << child_process->temp_key_path() << ". Error code: 0x" << result;
    return 1;
  }

  // Test with nonexistent key.
  std::vector<wchar_t> nonexistent_key_with_nulls =
      CreateVectorWithNulls(L"nonexistent0key0with0nulls");

  HANDLE reg_handle;
  result = proxy->NtOpenReadOnlyRegistry(
      root_handle,
      String16EmbeddedNulls(nonexistent_key_with_nulls.data(),
                            nonexistent_key_with_nulls.size()),
      KEY_READ, &reg_handle);
  if (reg_handle != INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Got a valid handle when trying to open a fake key.";
    return 1;
  }
  if (static_cast<NTSTATUS>(result) != STATUS_OBJECT_NAME_NOT_FOUND) {
    LOG(ERROR) << std::hex
               << "Got unexpected return code when opening a fake key. "
                  "Expected STATUS_OBJECT_NAME_NOT_FOUND(0x"
               << STATUS_OBJECT_NAME_NOT_FOUND << ") and got 0x" << result;
    return 1;
  }

  // Test with embedded nulls and null terminator.
  std::vector<wchar_t> key_with_nulls = CreateVectorWithNulls(kKeyWithNulls);
  result = proxy->NtOpenReadOnlyRegistry(
      root_handle,
      String16EmbeddedNulls(key_with_nulls.data(), key_with_nulls.size()),
      KEY_READ, &reg_handle);
  if (reg_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << std::hex << "Failed to get a valid registry handle for "
               << FormatVectorWithNulls(key_with_nulls) << ". Error code: 0x"
               << result;
    return 1;
  }

  // Test with missing null terminator.
  if (key_with_nulls.back() != L'\0') {
    LOG(ERROR) << "CreateVectorWithNulls skipped the null terminator in "
               << FormatVectorWithNulls(key_with_nulls);
    return 1;
  }
  std::vector<wchar_t> truncated_key_with_nulls(key_with_nulls.begin(),
                                                key_with_nulls.end() - 1);
  result = proxy->NtOpenReadOnlyRegistry(
      root_handle,
      String16EmbeddedNulls(truncated_key_with_nulls.data(),
                            truncated_key_with_nulls.size()),
      KEY_READ, &reg_handle);
  if (reg_handle != INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Got a valid registry handle for "
               << FormatVectorWithNulls(truncated_key_with_nulls)
               << " that has no null-terminator.";
    return 1;
  }
  if (result != SandboxErrorCode::INVALID_SUBKEY_STRING) {
    LOG(ERROR) << std::hex << "Got unexpected return code for "
               << FormatVectorWithNulls(truncated_key_with_nulls)
               << " that has no null-terminator. Expected "
               << "SandboxErrorCode::INVALID_SUBKEY_STRING (0x"
               << SandboxErrorCode::INVALID_SUBKEY_STRING << ") and got 0x"
               << result;
    return 1;
  }

  // Test with absolute path.
  base::string16 temp_key_full_path = child_process->temp_key_full_path();
  std::vector<wchar_t> full_path(temp_key_full_path.begin(),
                                 temp_key_full_path.end());
  full_path.push_back(L'\\');
  full_path.insert(full_path.end(), key_with_nulls.begin(),
                   key_with_nulls.end());
  result = proxy->NtOpenReadOnlyRegistry(
      nullptr, String16EmbeddedNulls(full_path.data(), full_path.size()),
      KEY_READ, &reg_handle);
  if (reg_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << std::hex << "Failed to get a valid registry handle for "
               << FormatVectorWithNulls(full_path) << ". Error code: 0x"
               << result;
    return 1;
  }

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(NtOpenReadOnlyRegistryNoHangs) {
  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<EngineRequestsProxy> proxy(
      child_process->GetEngineRequestsProxy());

  base::string16 too_long(std::numeric_limits<int16_t>::max() + 1, '0');
  HANDLE reg_handle;
  EXPECT_EQ(SandboxErrorCode::INVALID_SUBKEY_STRING,
            proxy->NtOpenReadOnlyRegistry(
                nullptr, String16EmbeddedNulls(too_long), 0, &reg_handle));

  child_process->UnbindRequestsRemotes();

  EXPECT_EQ(SandboxErrorCode::INTERNAL_ERROR,
            proxy->NtOpenReadOnlyRegistry(nullptr, String16EmbeddedNulls(), 0,
                                          &reg_handle));

  return ::testing::Test::HasNonfatalFailure();
}

using TestParentProcess = MaybeSandboxedParentProcess<SandboxedParentProcess>;

// EngineRequestsProxyTest is parametrized with:
//  - expected_exit_code_: expected exit code of the child process;
//  - child_main_function_: the name of the MULTIPROCESS_TEST_MAIN function for
//    the child process.
typedef std::tuple<int, std::string> EngineRequestsProxyTestParams;

class EngineRequestsProxyTest
    : public ::testing::TestWithParam<EngineRequestsProxyTestParams> {
 public:
  void SetUp() override {
    expected_exit_code_ = std::get<0>(GetParam());
    child_main_function_ = std::get<1>(GetParam());

    mojo_task_runner_ = MojoTaskRunner::Create();

    parent_process_ = base::MakeRefCounted<TestParentProcess>(
        mojo_task_runner_,
        TestParentProcess::CallbacksToSetup::kScanAndCleanupRequests);
  }

 protected:
  int expected_exit_code_;
  std::string child_main_function_;

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  scoped_refptr<TestParentProcess> parent_process_;
};

TEST_P(EngineRequestsProxyTest, TestRequest) {
  base::test::TaskEnvironment task_environment;

  // Create resources that tests running in the sandbox will not have access to
  // create for themselves, even before calling LowerToken.
  chrome_cleaner_sandbox::ScopedTempRegistryKey temp_key;
  parent_process_->AppendSwitchNative(kTempKeyPathSwitch, temp_key.Path());
  parent_process_->AppendSwitchNative(kTempKeyFullPathSwitch,
                                      temp_key.FullyQualifiedPath());

  std::vector<wchar_t> key_with_nulls = CreateVectorWithNulls(kKeyWithNulls);
  ULONG disposition = 0;
  HANDLE temp_handle = INVALID_HANDLE_VALUE;
  chrome_cleaner_sandbox::NativeCreateKey(temp_key.Get(), &key_with_nulls,
                                          &temp_handle, &disposition);

  // ScopedTempRegistryKey's destructor will try to delete this key using
  // win::RegKey, which does not handle embedded nulls. So it must be deleted
  // manually.
  // TODO(joenotcharles): ScopedTempRegistryKey should do this automatically.
  base::ScopedClosureRunner delete_temp_key(base::BindOnce(
      [](HANDLE handle) {
        chrome_cleaner_sandbox::NativeDeleteKey(handle);
        ::CloseHandle(handle);
      },
      temp_handle));

  const base::FilePath windows_dir =
      PreFetchedPaths::GetInstance()->GetWindowsFolder();
  parent_process_->AppendSwitchPath(kWindowsDirectorySwitch, windows_dir);

  int32_t exit_code = -1;
  EXPECT_TRUE(parent_process_->LaunchConnectedChildProcess(child_main_function_,
                                                           &exit_code));
  EXPECT_EQ(expected_exit_code_, exit_code);
}

INSTANTIATE_TEST_SUITE_P(
    Success,
    EngineRequestsProxyTest,
    testing::Combine(testing::Values(0),
                     testing::Values("GetFileAttributesTest",
                                     "GetFileAttributesNoHangs",
                                     "GetKnownFolderPath",
                                     "GetKnownFolderPathNoHangs",
                                     "GetProcesses",
                                     "GetProcessesNoHangs",
                                     "GetTasks",
                                     "GetTasksNoHangs",
                                     "GetProcessImagePath",
                                     "GetProcessImagePathNoHangs",
                                     "GetLoadedModules",
                                     "GetLoadedModulesNoHangs",
                                     "GetProcessCommandLine",
                                     "GetProcessCommandLineNoHangs",
                                     "GetUserInfoFromSID",
                                     "GetUserInfoFromSIDNoHangs",
                                     "OpenReadOnlyRegistry",
                                     "OpenReadOnlyRegistryNoHangs",
                                     "NtOpenReadOnlyRegistry",
                                     "NtOpenReadOnlyRegistryNoHangs")),
    GetParamNameForTest());

INSTANTIATE_TEST_SUITE_P(
    ConnectionError,
    EngineRequestsProxyTest,
    testing::Combine(
        testing::Values(TestChildProcess::kConnectionErrorExitCode),
        testing::Values("GetKnownFolderPathInvalidParam")),
    GetParamNameForTest());

}  // namespace

}  // namespace chrome_cleaner
