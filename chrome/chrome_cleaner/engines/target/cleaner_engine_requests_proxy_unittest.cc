// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/cleaner_engine_requests_proxy.h"

#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "chrome/chrome_cleaner/engines/common/registry_util.h"
#include "chrome/chrome_cleaner/engines/target/sandboxed_test_helpers.h"
#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/test/scoped_process_protector.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_native_reg_util.h"
#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_task_scheduler.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

// Switches with information about resources to pass to the subprocess.
constexpr char kLongRunningProcessIdSwitch[] = "test-process-id";
constexpr char kServiceNameSwitch[] = "test-service-name";
constexpr char kTempDirectoryPathSwitch[] = "test-temp-dir";
constexpr char kTempRegistryKeyPath[] = "test-temp-key-path";

// Constants shared with the subprocess.
constexpr wchar_t kTempFileName[] = L"temp_file.exe";
constexpr wchar_t kMissingFileName[] = L"missing_file.exe";
constexpr wchar_t kRegistryKeyWithNulls[] = L"ab\0c";
constexpr size_t kRegistryKeyWithNullsLength = 5;  // Including trailing null.
constexpr wchar_t kRegistryValueNameWithNulls[] = L"fo\0o";
constexpr size_t kRegistryValueNameWithNullsLength =
    5;  // Including trailing null.
constexpr wchar_t kRegistryValueWithNulls[] = L"b\0ar";
constexpr size_t kRegistryValueWithNullsLength = 5;  // Including trailing null.

scoped_refptr<SandboxChildProcess> SetupChildProcess() {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  auto child_process =
      base::MakeRefCounted<SandboxChildProcess>(mojo_task_runner);
  child_process->LowerToken();
  return child_process;
}

base::ProcessId GetTestProcessId(const base::CommandLine& command_line) {
  base::string16 pid_string =
      command_line.GetSwitchValueNative(kLongRunningProcessIdSwitch);
  uint64_t pid;
  if (!base::StringToUint64(pid_string, &pid)) {
    LOG(ERROR) << "Invalid process id switch: " << pid_string;
    return 0;
  }
  return base::checked_cast<base::ProcessId>(pid);
}

base::FilePath GetTestFilePath(const base::CommandLine& command_line,
                               const base::string16& file_name) {
  base::FilePath path =
      command_line.GetSwitchValuePath(kTempDirectoryPathSwitch);
  if (path.empty()) {
    LOG(ERROR) << "Missing temp directory path switch";
    return path;
  }
  return path.Append(file_name);
}

String16EmbeddedNulls GetTestRegistryKeyPath(
    const base::CommandLine& command_line) {
  base::string16 path = command_line.GetSwitchValueNative(kTempRegistryKeyPath);
  if (path.empty()) {
    LOG(ERROR) << "Missing temp registry key path switch";
    return String16EmbeddedNulls();
  }
  path += L"\\";
  path.append(kRegistryKeyWithNulls, kRegistryKeyWithNullsLength);
  return String16EmbeddedNulls(path);
}

String16EmbeddedNulls GetTestRegistryValueName() {
  return String16EmbeddedNulls(kRegistryValueNameWithNulls,
                               kRegistryValueNameWithNullsLength);
}

String16EmbeddedNulls GetTestRegistryValue() {
  return String16EmbeddedNulls(kRegistryValueWithNulls,
                               kRegistryValueWithNullsLength);
}

class CleanerEngineRequestsProxyTestBase : public ::testing::Test {
 public:
  using TestParentProcess = MaybeSandboxedParentProcess<SandboxedParentProcess>;

  void SetUp() override {
    mojo_task_runner_ = MojoTaskRunner::Create();

    parent_process_ = base::MakeRefCounted<TestParentProcess>(
        mojo_task_runner_,
        TestParentProcess::CallbacksToSetup::kCleanupRequests);
  }

 protected:
  ::testing::AssertionResult LaunchConnectedChildProcess(
      const std::string& child_main_function,
      int32_t expected_exit_code = 0) {
    int32_t exit_code = -1;
    if (!parent_process_->LaunchConnectedChildProcess(child_main_function,
                                                      &exit_code)) {
      return ::testing::AssertionFailure()
             << "Failed to launch child process for " << child_main_function;
    }
    if (exit_code != expected_exit_code) {
      return ::testing::AssertionFailure()
             << "Got exit code " << exit_code << ", expected "
             << expected_exit_code;
    }
    return ::testing::AssertionSuccess();
  }

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  scoped_refptr<TestParentProcess> parent_process_;

 private:
  base::test::TaskEnvironment task_environment_;
};

// CleanerEngineRequestsProxyTest is parameterized with:
//  - resource_status_: how the test is expected to affect its resources;
//  - child_main_function_: the name of the MULTIPROCESS_TEST_MAIN function for
//    the child process.
enum class ResourceStatus {
  kUnspecified,  // For tests that don't check the resource status.
  kNoChange,
  kDeleted,
};

std::ostream& operator<<(std::ostream& stream, ResourceStatus status) {
  return stream << static_cast<int>(status);
}

typedef std::tuple<ResourceStatus, std::string>
    CleanerEngineRequestsProxyTestParams;

/*
 * Tests Without Extra Setup
 */

class CleanerEngineRequestsProxyTest
    : public CleanerEngineRequestsProxyTestBase,
      public ::testing::WithParamInterface<
          CleanerEngineRequestsProxyTestParams> {
 public:
  void SetUp() override {
    std::tie(resource_status_, child_main_function_) = GetParam();
    CleanerEngineRequestsProxyTestBase::SetUp();
  }

 protected:
  ResourceStatus resource_status_ = ResourceStatus::kUnspecified;
  std::string child_main_function_;
};

MULTIPROCESS_TEST_MAIN(DeleteTask) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  TestTaskScheduler test_task_scheduler;

  TaskScheduler::TaskInfo task_info;
  if (!RegisterTestTask(&test_task_scheduler, &task_info)) {
    LOG(ERROR) << "Failed to register a test task";
    return 1;
  }

  EXPECT_TRUE(proxy->DeleteTask(task_info.name));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(DeleteTaskNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());
  child_process->UnbindRequestsRemotes();

  TestTaskScheduler test_task_scheduler;

  TaskScheduler::TaskInfo task_info;
  if (!RegisterTestTask(&test_task_scheduler, &task_info)) {
    LOG(ERROR) << "Failed to register a test task";
    return 1;
  }

  EXPECT_FALSE(proxy->DeleteTask(task_info.name));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_P(CleanerEngineRequestsProxyTest, TestRequest) {
  EXPECT_TRUE(LaunchConnectedChildProcess(child_main_function_));
}

INSTANTIATE_TEST_SUITE_P(
    ProxyTests,
    CleanerEngineRequestsProxyTest,
    testing::Combine(testing::Values(ResourceStatus::kUnspecified),
                     testing::Values("DeleteTask", "DeleteTaskNoHang")),
    GetParamNameForTest());

/*
 * File Tests
 */

class CleanerEngineRequestsProxyFileTest
    : public CleanerEngineRequestsProxyTest {
 public:
  void SetUp() override {
    CleanerEngineRequestsProxyTest::SetUp();
    ASSERT_TRUE(parent_process_);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    parent_process_->AppendSwitchPath(kTempDirectoryPathSwitch,
                                      temp_dir_.GetPath());

    ASSERT_TRUE(CreateEmptyFile(temp_dir_.GetPath().Append(kTempFileName)));
    ASSERT_TRUE(CreateEmptyFile(temp_dir_.GetPath().Append(kValidUtf8Name)));
    ASSERT_TRUE(CreateEmptyFile(temp_dir_.GetPath().Append(kInvalidUtf8Name)));
  }

 protected:
  base::ScopedTempDir temp_dir_;
};

MULTIPROCESS_TEST_MAIN(DeleteFileBasic) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  EXPECT_TRUE(proxy->DeleteFile(
      GetTestFilePath(child_process->command_line(), kTempFileName)));

  // Succeeds on absent files.
  EXPECT_TRUE(proxy->DeleteFile(
      GetTestFilePath(child_process->command_line(), kMissingFileName)));

  EXPECT_TRUE(proxy->DeleteFile(
      GetTestFilePath(child_process->command_line(), kValidUtf8Name)));

  EXPECT_TRUE(proxy->DeleteFile(
      GetTestFilePath(child_process->command_line(), kInvalidUtf8Name)));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(DeleteFileNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());
  child_process->UnbindRequestsRemotes();

  EXPECT_FALSE(proxy->DeleteFile(
      GetTestFilePath(child_process->command_line(), kTempFileName)));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(DeleteFilePostReboot) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());
  EXPECT_TRUE(proxy->DeleteFilePostReboot(
      GetTestFilePath(child_process->command_line(), kTempFileName)));

  // Succeeds on absent files.
  EXPECT_TRUE(proxy->DeleteFilePostReboot(
      GetTestFilePath(child_process->command_line(), kMissingFileName)));

  EXPECT_TRUE(proxy->DeleteFilePostReboot(
      GetTestFilePath(child_process->command_line(), kValidUtf8Name)));

  EXPECT_TRUE(proxy->DeleteFilePostReboot(
      GetTestFilePath(child_process->command_line(), kInvalidUtf8Name)));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(DeleteFilePostRebootNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());
  child_process->UnbindRequestsRemotes();

  EXPECT_FALSE(proxy->DeleteFilePostReboot(
      GetTestFilePath(child_process->command_line(), kTempFileName)));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_P(CleanerEngineRequestsProxyFileTest, TestRequest) {
  EXPECT_TRUE(LaunchConnectedChildProcess(child_main_function_));

  bool files_exist = resource_status_ != ResourceStatus::kDeleted;
  EXPECT_EQ(base::PathExists(temp_dir_.GetPath().Append(kTempFileName)),
            files_exist);
  EXPECT_EQ(base::PathExists(temp_dir_.GetPath().Append(kValidUtf8Name)),
            files_exist);
  EXPECT_EQ(base::PathExists(temp_dir_.GetPath().Append(kInvalidUtf8Name)),
            files_exist);
  EXPECT_FALSE(base::PathExists(temp_dir_.GetPath().Append(kMissingFileName)));
}

INSTANTIATE_TEST_SUITE_P(
    FilesDeleted,
    CleanerEngineRequestsProxyFileTest,
    testing::Combine(testing::Values(ResourceStatus::kDeleted),
                     testing::Values("DeleteFileBasic")),
    GetParamNameForTest());

// For post-reboot tests, files aren't actually deleted until after reboot, so
// they will still exist even if the test passes.
INSTANTIATE_TEST_SUITE_P(
    FilesRemain,
    CleanerEngineRequestsProxyFileTest,
    testing::Combine(testing::Values(ResourceStatus::kNoChange),
                     testing::Values("DeleteFileNoHang",
                                     "DeleteFilePostReboot",
                                     "DeleteFilePostRebootNoHang")),
    GetParamNameForTest());

/*
 * Registry Tests
 */

class CleanerEngineRequestsProxyRegistryTest
    : public CleanerEngineRequestsProxyTest {
 public:
  void SetUp() override {
    CleanerEngineRequestsProxyTest::SetUp();
    ASSERT_TRUE(parent_process_);

    std::vector<wchar_t> key_name(
        kRegistryKeyWithNulls,
        kRegistryKeyWithNulls + kRegistryKeyWithNullsLength);

    ULONG disposition = 0;
    ASSERT_EQ(chrome_cleaner_sandbox::NativeCreateKey(
                  temp_key_.Get(), &key_name, &temp_key_handle_, &disposition),
              STATUS_SUCCESS);
    ASSERT_EQ(disposition, static_cast<ULONG>(REG_CREATED_NEW_KEY));
    ASSERT_NE(temp_key_handle_, INVALID_HANDLE_VALUE);

    parent_process_->AppendSwitchNative(kTempRegistryKeyPath,
                                        temp_key_.FullyQualifiedPath());

    String16EmbeddedNulls value_name_buffer{L'f', L'o', L'o', L'\0'};
    String16EmbeddedNulls value_buffer{L'b', L'a', L'r', L'\0'};
    ASSERT_EQ(chrome_cleaner_sandbox::NativeSetValueKey(
                  temp_key_handle_, GetTestRegistryValueName(), REG_SZ,
                  GetTestRegistryValue()),
              STATUS_SUCCESS);
  }

  void TearDown() override {
    // Clean up the test key if it hasn't already been deleted.
    ASSERT_NE(temp_key_handle_, INVALID_HANDLE_VALUE);
    chrome_cleaner_sandbox::NativeDeleteKey(temp_key_handle_);
    ::CloseHandle(temp_key_handle_);
  }

 protected:
  chrome_cleaner_sandbox::ScopedTempRegistryKey temp_key_;
  HANDLE temp_key_handle_ = INVALID_HANDLE_VALUE;
};

MULTIPROCESS_TEST_MAIN(NtDeleteRegistryKey) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  EXPECT_TRUE(proxy->NtDeleteRegistryKey(
      GetTestRegistryKeyPath(child_process->command_line())));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(NtDeleteRegistryKeyNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  EXPECT_FALSE(proxy->NtDeleteRegistryKey(String16EmbeddedNulls(nullptr)));

  child_process->UnbindRequestsRemotes();

  EXPECT_FALSE(proxy->NtDeleteRegistryKey(
      GetTestRegistryKeyPath(child_process->command_line())));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(NtDeleteRegistryValue) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  EXPECT_TRUE(proxy->NtDeleteRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      GetTestRegistryValueName()));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(NtDeleteRegistryValueNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  EXPECT_FALSE(proxy->NtDeleteRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      String16EmbeddedNulls(nullptr)));
  EXPECT_FALSE(proxy->NtDeleteRegistryValue(String16EmbeddedNulls(nullptr),
                                            GetTestRegistryValueName()));

  child_process->UnbindRequestsRemotes();

  EXPECT_FALSE(proxy->NtDeleteRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      GetTestRegistryValueName()));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(NtChangeRegistryValue) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  const String16EmbeddedNulls new_value(
      GetTestRegistryValue().CastAsStringPiece16().substr(1));

  EXPECT_TRUE(proxy->NtChangeRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      GetTestRegistryValueName(), new_value));

  // Remove the terminating null char from the new value and ensure it still
  // works.
  EXPECT_TRUE(proxy->NtChangeRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      GetTestRegistryValueName(),
      String16EmbeddedNulls(new_value.CastAsWCharArray(), new_value.size())));

  // Set the new value to an empty string.
  EXPECT_TRUE(proxy->NtChangeRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      GetTestRegistryValueName(), String16EmbeddedNulls()));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(NtChangeRegistryValueNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  const String16EmbeddedNulls new_value(
      GetTestRegistryValue().CastAsStringPiece16().substr(1));

  EXPECT_FALSE(proxy->NtChangeRegistryValue(
      String16EmbeddedNulls(nullptr), GetTestRegistryValueName(), new_value));
  EXPECT_FALSE(proxy->NtChangeRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      String16EmbeddedNulls(nullptr), new_value));

  child_process->UnbindRequestsRemotes();

  EXPECT_FALSE(proxy->NtChangeRegistryValue(
      GetTestRegistryKeyPath(child_process->command_line()),
      GetTestRegistryValueName(), new_value));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_P(CleanerEngineRequestsProxyRegistryTest, TestRequest) {
  EXPECT_TRUE(LaunchConnectedChildProcess(child_main_function_));
}

INSTANTIATE_TEST_SUITE_P(
    ProxyTests,
    CleanerEngineRequestsProxyRegistryTest,
    testing::Combine(testing::Values(ResourceStatus::kUnspecified),
                     testing::Values("NtDeleteRegistryKey",
                                     "NtDeleteRegistryKeyNoHang",
                                     "NtDeleteRegistryValue",
                                     "NtDeleteRegistryValueNoHang",
                                     "NtChangeRegistryValue",
                                     "NtChangeRegistryValueNoHang")),
    GetParamNameForTest());

/*
 * Service Tests
 */

class CleanerEngineRequestsProxyServiceTest
    : public CleanerEngineRequestsProxyTest {
 public:
  void SetUp() override {
    CleanerEngineRequestsProxyTest::SetUp();
    ASSERT_TRUE(parent_process_);

    ASSERT_TRUE(EnsureNoTestServicesRunning());

    ASSERT_TRUE(service_handle_.InstallService());
    service_handle_.Close();

    parent_process_->AppendSwitchNative(kServiceNameSwitch,
                                        service_handle_.service_name());
  }

 protected:
  TestScopedServiceHandle service_handle_;
};

MULTIPROCESS_TEST_MAIN(DeleteService) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  base::string16 service_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kServiceNameSwitch);
  CHECK(!service_name.empty());

  EXPECT_TRUE(proxy->DeleteService(service_name));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(DeleteServiceNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());
  child_process->UnbindRequestsRemotes();

  base::string16 service_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kServiceNameSwitch);
  CHECK(!service_name.empty());

  EXPECT_FALSE(proxy->DeleteService(service_name));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_P(CleanerEngineRequestsProxyServiceTest, TestRequest) {
  EXPECT_TRUE(LaunchConnectedChildProcess(child_main_function_));

  bool service_exists = resource_status_ != ResourceStatus::kDeleted;
  EXPECT_EQ(DoesServiceExist(service_handle_.service_name()), service_exists);
}

INSTANTIATE_TEST_SUITE_P(
    ServiceDeleted,
    CleanerEngineRequestsProxyServiceTest,
    testing::Combine(testing::Values(ResourceStatus::kDeleted),
                     testing::Values("DeleteService")),
    GetParamNameForTest());

INSTANTIATE_TEST_SUITE_P(
    ServiceRemains,
    CleanerEngineRequestsProxyServiceTest,
    testing::Combine(testing::Values(ResourceStatus::kNoChange),
                     testing::Values("DeleteServiceNoHang")),
    GetParamNameForTest());

/*
 * Terminate Process Tests
 */

class CleanerEngineRequestsProxyTerminateTest
    : public CleanerEngineRequestsProxyTestBase {
 public:
  void SetUp() override {
    // Note that this test will fail under the debugger since the debugged test
    // process will inherit the SeDebugPrivilege which allows the test to get
    // an ALL_ACCESS handle. So if under the debugger, skip the test without
    // failing.
    if (::IsDebuggerPresent()) {
      LOG(ERROR) << "TerminateProcessTest skipped when running in debugger.";
      silently_skip_ = true;
      return;
    }

    CleanerEngineRequestsProxyTestBase::SetUp();
    ASSERT_TRUE(parent_process_);

    test_process_ = LongRunningProcess(/*command_line=*/nullptr);
    ASSERT_TRUE(test_process_.IsValid());

    base::string16 switch_str = base::NumberToString16(test_process_.Pid());
    parent_process_->AppendSwitchNative(kLongRunningProcessIdSwitch,
                                        switch_str);
  }

  void TearDown() override {
    // Clean up if the test didn't kill the process.
    if (test_process_.IsValid())
      test_process_.Terminate(2, false);
  }

 protected:
  ::testing::AssertionResult WaitForChildProcessToDie() {
    int test_process_exit_code = 0;
    if (!test_process_.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                              &test_process_exit_code)) {
      return ::testing::AssertionFailure() << "Child process did not exit";
    }
    if (test_process_exit_code != 1) {
      return ::testing::AssertionFailure()
             << "Child process exited with unexpected exit code "
             << test_process_exit_code;
    }
    return ::testing::AssertionSuccess();
  }

  ::testing::AssertionResult ChildProcessIsRunning() {
    DWORD test_process_exit_code = 420042;
    if (!::GetExitCodeProcess(test_process_.Handle(),
                              &test_process_exit_code)) {
      return ::testing::AssertionFailure()
             << "Error polling for exit for of child process";
    }
    if (test_process_exit_code != STILL_ACTIVE) {
      return ::testing::AssertionFailure()
             << "Child process exited with unexpected exit code "
             << test_process_exit_code << " (expected STILL_ACTIVE)";
    }
    return ::testing::AssertionSuccess();
  }

  base::Process test_process_;

  bool silently_skip_ = false;
};

MULTIPROCESS_TEST_MAIN(TerminateProcess) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  base::ProcessId pid = GetTestProcessId(child_process->command_line());
  if (!pid) {
    LOG(ERROR) << "Couldn't find test process";
    return 1;
  }

  EXPECT_TRUE(proxy->TerminateProcess(pid));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(TerminateProcessProtected) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());

  base::ProcessId pid = GetTestProcessId(child_process->command_line());
  if (!pid) {
    LOG(ERROR) << "Couldn't find test process";
    return 1;
  }

  // We should not be able to kill the protected process.
  EXPECT_FALSE(proxy->TerminateProcess(pid));

  return ::testing::Test::HasNonfatalFailure();
}

MULTIPROCESS_TEST_MAIN(TerminateProcessNoHang) {
  auto child_process = SetupChildProcess();
  if (!child_process)
    return 1;

  scoped_refptr<CleanerEngineRequestsProxy> proxy(
      child_process->GetCleanerEngineRequestsProxy());
  child_process->UnbindRequestsRemotes();

  base::ProcessId pid = GetTestProcessId(child_process->command_line());
  if (!pid) {
    LOG(ERROR) << "Couldn't find test process";
    return 1;
  }

  EXPECT_FALSE(proxy->TerminateProcess(pid));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_F(CleanerEngineRequestsProxyTerminateTest, TerminateProcess) {
  if (silently_skip_)
    return;

  EXPECT_TRUE(LaunchConnectedChildProcess("TerminateProcess"));
  EXPECT_TRUE(WaitForChildProcessToDie());
}

TEST_F(CleanerEngineRequestsProxyTerminateTest, TerminateProcessProtected) {
  if (silently_skip_)
    return;

  ScopedProcessProtector process_protector(test_process_.Pid());

  EXPECT_TRUE(LaunchConnectedChildProcess("TerminateProcessProtected"));
  EXPECT_TRUE(ChildProcessIsRunning());
}

TEST_F(CleanerEngineRequestsProxyTerminateTest, TerminateProcessNoHang) {
  if (silently_skip_)
    return;

  EXPECT_TRUE(LaunchConnectedChildProcess("TerminateProcessNoHang"));
  EXPECT_TRUE(ChildProcessIsRunning());
}

}  // namespace

}  // namespace chrome_cleaner
