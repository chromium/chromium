// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <atomic>
#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/sandboxed_test_helpers.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/engine_file_requests.mojom.h"
#include "chrome/chrome_cleaner/os/inheritable_event.h"
#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_native_reg_util.h"
#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"
#include "chrome/chrome_cleaner/test/test_task_scheduler.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "sandbox/win/src/sid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

constexpr char kSharedEventHandleSwitch[] = "shared-event-handle";
constexpr char kTestRequestNameSwitch[] = "test-request-name";
constexpr char kTestFilePathSwitch[] = "test-file-path";
constexpr char kTestRegKeySwitch[] = "test-reg-key";
constexpr char kTestProcessIdSwitch[] = "test-process-id";

// An InterfaceMetadataObserver that introduces a pause into the first call
// that's observed, to simulate a slow operation.
class SlowMetadataObserver : public InterfaceMetadataObserver {
 public:
  explicit SlowMetadataObserver(base::WaitableEvent* event) : event_(event) {}
  ~SlowMetadataObserver() override = default;

  // InterfaceMetadataObserver

  void ObserveCall(const LogInformation& log_information,
                   const std::map<std::string, std::string>& params) override {
    // Ignore the params.
    ObserveCall(log_information);
  }

  void ObserveCall(const LogInformation& log_information) override {
    bool was_first_call = is_first_call_.exchange(false);
    if (was_first_call) {
      // Wait for longer than TestTimeouts::action_timeout() to avoid a race
      // condition if the second call times out at the same time as this. The
      // child process will signal |event_| to abort the wait when the test is
      // done.
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;
      event_->TimedWait(TestTimeouts::action_max_timeout());
    }
  }

 private:
  std::atomic<bool> is_first_call_{true};
  base::WaitableEvent* event_;
};

scoped_refptr<SandboxChildProcess> SetupSandboxedChildProcess() {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  auto child_process =
      base::MakeRefCounted<SandboxChildProcess>(mojo_task_runner);
  child_process->LowerToken();
  return child_process;
}

// Execute |closure| on a different sequence since it could block and we don't
// want to block on the Mojo thread.
void InvokeOnOtherSequence(base::OnceClosure closure) {
  base::PostTask(FROM_HERE,
                 {base::ThreadPool(), base::WithBaseSyncPrimitives()},
                 std::move(closure));
}

}  // namespace

class TestEngineRequestInvoker {
 public:
  TestEngineRequestInvoker(SandboxChildProcess* child_process,
                           const base::FilePath& test_file_path,
                           const String16EmbeddedNulls& test_native_reg_key,
                           base::ProcessId test_process_id)
      : file_requests_proxy_(child_process->GetFileRequestsProxy()),
        engine_requests_proxy_(child_process->GetEngineRequestsProxy()),
        cleaner_requests_proxy_(child_process->GetCleanerEngineRequestsProxy()),
        test_file_path_(test_file_path),
        test_native_reg_key_(test_native_reg_key),
        test_process_id_(test_process_id) {}

  // Invokes a request on the proxy object. The proxy will send the request
  // over Mojo to the parent process, where the request's impl may pause for a
  // long time. After the response is received over Mojo, |result_closure| is
  // called on another sequence to avoid blocking the Mojo thread. The
  // request's actual result is ignored since this test only cares about the
  // timing when it's received.
  void InvokeRequest(const std::string& request_name,
                     base::OnceClosure result_closure) {
    using base::BindOnce;
    using base::IgnoreResult;
    using FileProxy = EngineFileRequestsProxy;
    using EngineProxy = EngineRequestsProxy;
    using CleanerProxy = CleanerEngineRequestsProxy;

    // Functions called with these parameters may fail but that's ok as long as
    // they don't block while they're failing.
    constexpr FindFileHandle kDummyFindFileHandle = -1;  // INVALID_HANDLE_VALUE

    // This is static because it needs to exist for the entire life of an async
    // request.
    static const sandbox::Sid kDummySid(WinSelfSid);

    if (request_name == "FindFirstFile") {
      file_requests_proxy_->task_runner()->PostTask(
          FROM_HERE, BindOnce(IgnoreResult(&FileProxy::SandboxFindFirstFile),
                              file_requests_proxy_, test_file_path_,
                              BindOnce(&FindFirstFileCallback,
                                       std::move(result_closure))));
    } else if (request_name == "FindNextFile") {
      file_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&FileProxy::SandboxFindNextFile),
                   file_requests_proxy_, kDummyFindFileHandle,
                   BindOnce(&FindNextFileCallback, std::move(result_closure))));
    } else if (request_name == "FindClose") {
      file_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&FileProxy::SandboxFindClose),
                   file_requests_proxy_, kDummyFindFileHandle,
                   BindOnce(&FindCloseCallback, std::move(result_closure))));
    } else if (request_name == "OpenReadOnlyFile") {
      file_requests_proxy_->task_runner()->PostTask(
          FROM_HERE, BindOnce(IgnoreResult(&FileProxy::SandboxOpenReadOnlyFile),
                              file_requests_proxy_, test_file_path_, 0,
                              BindOnce(&OpenReadOnlyFileCallback,
                                       std::move(result_closure))));
    } else if (request_name == "GetFileAttributes") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(
              IgnoreResult(&EngineProxy::SandboxGetFileAttributes),
              engine_requests_proxy_, test_file_path_,
              BindOnce(&GetFileAttributesCallback, std::move(result_closure))));
    } else if (request_name == "GetKnownFolderPath") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxGetKnownFolderPath),
                   engine_requests_proxy_, mojom::KnownFolder::kWindows,
                   BindOnce(&GetKnownFolderPathCallback,
                            std::move(result_closure))));
    } else if (request_name == "GetProcesses") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxGetProcesses),
                   engine_requests_proxy_,
                   BindOnce(&GetProcessesCallback, std::move(result_closure))));
    } else if (request_name == "GetTasks") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxGetTasks),
                   engine_requests_proxy_,
                   BindOnce(&GetTasksCallback, std::move(result_closure))));
    } else if (request_name == "GetProcessImagePath") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxGetProcessImagePath),
                   engine_requests_proxy_, test_process_id_,
                   BindOnce(&GetProcessImagePathCallback,
                            std::move(result_closure))));
    } else if (request_name == "GetLoadedModules") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(
              IgnoreResult(&EngineProxy::SandboxGetLoadedModules),
              engine_requests_proxy_, test_process_id_,
              BindOnce(&GetLoadedModulesCallback, std::move(result_closure))));
    } else if (request_name == "GetProcessCommandLine") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxGetProcessCommandLine),
                   engine_requests_proxy_, test_process_id_,
                   BindOnce(&GetProcessCommandLineCallback,
                            std::move(result_closure))));
    } else if (request_name == "GetUserInfoFromSID") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxGetUserInfoFromSID),
                   engine_requests_proxy_,
                   static_cast<SID*>(kDummySid.GetPSID()),
                   BindOnce(&GetUserInfoFromSIDCallback,
                            std::move(result_closure))));
    } else if (request_name == "OpenReadOnlyRegistry") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxOpenReadOnlyRegistry),
                   engine_requests_proxy_, HKEY_CURRENT_USER,
                   L"Software\\Dummy", 0,
                   BindOnce(&OpenReadOnlyRegistryCallback,
                            std::move(result_closure))));
    } else if (request_name == "NtOpenReadOnlyRegistry") {
      engine_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&EngineProxy::SandboxNtOpenReadOnlyRegistry),
                   engine_requests_proxy_, nullptr, test_native_reg_key_, 0,
                   BindOnce(&NtOpenReadOnlyRegistryCallback,
                            std::move(result_closure))));
    } else if (request_name == "DeleteFile") {
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&CleanerProxy::SandboxDeleteFile),
                   cleaner_requests_proxy_, test_file_path_,
                   BindOnce(&DeleteFileCallback, std::move(result_closure))));
    } else if (request_name == "DeleteFilePostReboot") {
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&CleanerProxy::SandboxDeleteFilePostReboot),
                   cleaner_requests_proxy_, test_file_path_,
                   BindOnce(&DeleteFilePostRebootCallback,
                            std::move(result_closure))));
    } else if (request_name == "NtDeleteRegistryKey") {
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&CleanerProxy::SandboxNtDeleteRegistryKey),
                   cleaner_requests_proxy_, test_native_reg_key_,
                   BindOnce(&NtDeleteRegistryKeyCallback,
                            std::move(result_closure))));
    } else if (request_name == "NtDeleteRegistryValue") {
      // Reuse the key name for the value. It's an arbitrary string since the
      // key doesn't really exist.
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&CleanerProxy::SandboxNtDeleteRegistryValue),
                   cleaner_requests_proxy_, test_native_reg_key_,
                   /*value_name=*/test_native_reg_key_,
                   BindOnce(&NtDeleteRegistryValueCallback,
                            std::move(result_closure))));
    } else if (request_name == "NtChangeRegistryValue") {
      // Reuse the key name for the value. It's an arbitrary string since the
      // key doesn't really exist.
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&CleanerProxy::SandboxNtChangeRegistryValue),
                   cleaner_requests_proxy_, test_native_reg_key_,
                   /*value_name=*/test_native_reg_key_,
                   /*new_value=*/test_native_reg_key_,
                   BindOnce(&NtChangeRegistryValueCallback,
                            std::move(result_closure))));
    } else if (request_name == "DeleteService") {
      // The broker should reject the empty string so we won't risk deleting a
      // real service.
      const base::string16 empty_service_name;
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE, BindOnce(IgnoreResult(&CleanerProxy::SandboxDeleteService),
                              cleaner_requests_proxy_, empty_service_name,
                              BindOnce(&DeleteServiceCallback,
                                       std::move(result_closure))));
    } else if (request_name == "DeleteTask") {
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(IgnoreResult(&CleanerProxy::SandboxDeleteTask),
                   cleaner_requests_proxy_, L"NonExistentTaskName",
                   BindOnce(&DeleteTaskCallback, std::move(result_closure))));
    } else if (request_name == "TerminateProcess") {
      cleaner_requests_proxy_->task_runner()->PostTask(
          FROM_HERE,
          BindOnce(
              IgnoreResult(&CleanerProxy::SandboxTerminateProcess),
              cleaner_requests_proxy_, test_process_id_,
              BindOnce(&TerminateProcessCallback, std::move(result_closure))));

    } else {
      NOTREACHED() << "Unrecognized request: " << request_name;
    }
  }

 private:
  static void FindFirstFileCallback(
      base::OnceClosure closure,
      uint32_t /*result*/,
      chrome_cleaner::mojom::FindFileDataPtr /*win32_find_data*/,
      chrome_cleaner::mojom::FindHandlePtr /*find_handle*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void FindNextFileCallback(
      base::OnceClosure closure,
      uint32_t /*result*/,
      chrome_cleaner::mojom::FindFileDataPtr /*win32_find_data*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void FindCloseCallback(base::OnceClosure closure,
                                uint32_t /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void OpenReadOnlyFileCallback(base::OnceClosure closure,
                                       mojo::ScopedHandle /*handle*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetFileAttributesCallback(base::OnceClosure closure,
                                        uint32_t /*result*/,
                                        uint32_t /*attributes*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetKnownFolderPathCallback(base::OnceClosure closure,
                                         bool /*result*/,
                                         const base::FilePath& /*path*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetProcessesCallback(
      base::OnceClosure closure,
      bool /*result*/,
      const std::vector<base::ProcessId>& /*processes*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetTasksCallback(base::OnceClosure closure,
                               bool /*result*/,
                               std::vector<mojom::ScheduledTaskPtr> /*tasks*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetProcessImagePathCallback(base::OnceClosure closure,
                                          bool /*result*/,
                                          const base::FilePath& /*path*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetLoadedModulesCallback(
      base::OnceClosure closure,
      bool /*result*/,
      const std::vector<base::string16>& /*modules*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetProcessCommandLineCallback(
      base::OnceClosure closure,
      bool /*result*/,
      const base::string16& /*command_line*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void GetUserInfoFromSIDCallback(base::OnceClosure closure,
                                         bool /*result*/,
                                         mojom::UserInformationPtr /*info*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void OpenReadOnlyRegistryCallback(base::OnceClosure closure,
                                           uint32_t /*result*/,
                                           HANDLE /*handle*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void NtOpenReadOnlyRegistryCallback(base::OnceClosure closure,
                                             uint32_t /*result*/,
                                             HANDLE /*handle*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void DeleteFileCallback(base::OnceClosure closure, bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void DeleteFilePostRebootCallback(base::OnceClosure closure,
                                           bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void NtDeleteRegistryKeyCallback(base::OnceClosure closure,
                                          bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void NtDeleteRegistryValueCallback(base::OnceClosure closure,
                                            bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void NtChangeRegistryValueCallback(base::OnceClosure closure,
                                            bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void DeleteServiceCallback(base::OnceClosure closure,
                                    bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void DeleteTaskCallback(base::OnceClosure closure, bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  static void TerminateProcessCallback(base::OnceClosure closure,
                                       bool /*result*/) {
    InvokeOnOtherSequence(std::move(closure));
  }

  scoped_refptr<EngineFileRequestsProxy> file_requests_proxy_;
  scoped_refptr<EngineRequestsProxy> engine_requests_proxy_;
  scoped_refptr<CleanerEngineRequestsProxy> cleaner_requests_proxy_;

  base::FilePath test_file_path_;
  String16EmbeddedNulls test_native_reg_key_;
  base::ProcessId test_process_id_;
};

MULTIPROCESS_TEST_MAIN(EngineRequestsNoBlocking) {
  base::test::TaskEnvironment task_environment;

  auto child_process = SetupSandboxedChildProcess();
  if (!child_process)
    return 1;

  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string request_name =
      command_line->GetSwitchValueASCII(kTestRequestNameSwitch);

  base::string16 test_native_reg_key_str =
      command_line->GetSwitchValueNative(kTestRegKeySwitch);
  // Include the final null.
  String16EmbeddedNulls test_native_reg_key(test_native_reg_key_str.data(),
                                            test_native_reg_key_str.size() + 1);

  unsigned test_process_id;
  CHECK(base::StringToUint(
      command_line->GetSwitchValueNative(kTestProcessIdSwitch),
      &test_process_id));

  TestEngineRequestInvoker invoker(
      child_process.get(),
      command_line->GetSwitchValuePath(kTestFilePathSwitch),
      test_native_reg_key, base::ProcessId(test_process_id));

  // Make a call, which will go across the Mojo interface to the parent
  // process. Since this is the first call observed by the MetadataObserver in
  // the parent process, it should block in that process for longer than
  // action_timeout.
  base::WaitableEvent blocking_event;
  invoker.InvokeRequest(request_name,
                        base::BindOnce(&base::WaitableEvent::Signal,
                                       base::Unretained(&blocking_event)));

  base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;

  // Pause to be sure the call has started and is blocking. (Avoids a race
  // condition if calls are serviced in separate threads in the parent
  // process.)
  base::WaitableEvent pause_event;
  pause_event.TimedWait(TestTimeouts::tiny_timeout());

  // Ensure a second call works, even though the first hasn't completed.
  // When this call executes it will signal the event, which causes TimedWait to
  // return true. If the wait times out without executing the call, TimedWait
  // will return false.
  base::WaitableEvent nonblocking_event;
  invoker.InvokeRequest(request_name,
                        base::BindOnce(&base::WaitableEvent::Signal,
                                       base::Unretained(&nonblocking_event)));

  bool called_without_timeout =
      nonblocking_event.TimedWait(TestTimeouts::action_timeout());
  EXPECT_FALSE(blocking_event.IsSignaled())
      << "First Mojo call finished unexpectedly.";
  EXPECT_TRUE(called_without_timeout)
      << "First Mojo call seems to be blocking the thread.";

  // Cancel the blocking call and wait until it's cleaned up. This avoids a
  // race condition: after |shared_event| is signaled, the blocking call in the
  // broker process will return and the response is written over the mojo pipe,
  // which triggers the callback that signals |blocking_event|. We need to wait
  // until that response is received before we return from this function and
  // tear down the mojo pipe.
  uint32_t handle_value = 0;
  CHECK(base::StringToUint(
      command_line->GetSwitchValueNative(kSharedEventHandleSwitch),
      &handle_value));
  base::WaitableEvent shared_event(
      base::win::ScopedHandle(base::win::Uint32ToHandle(handle_value)));
  shared_event.Signal();
  blocking_event.Wait();

  return ::testing::Test::HasFailure();
}

using TestParentProcess = MaybeSandboxedParentProcess<SandboxedParentProcess>;

class EngineRequestsNoBlockingTest
    : public ::testing::TestWithParam<const char*> {};

TEST_P(EngineRequestsNoBlockingTest, TestRequest) {
  // All of these tests fail when run on win8 bots so return right away.
  // TODO(crbug.com/947576): Find out why and re-enable them.
  if (base::win::GetVersion() == base::win::Version::WIN8)
    return;

  base::test::TaskEnvironment task_environment;

  // This event will be shared between the parent and child processes. The
  // parent will wait on the event to simulate a long-running function call.
  // The child can signal the event to cancel the wait once the test is
  // finished.
  auto shared_event =
      CreateInheritableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  SlowMetadataObserver slow_metadata_observer(shared_event.get());

  scoped_refptr<TestParentProcess> parent_process =
      base::MakeRefCounted<TestParentProcess>(
          MojoTaskRunner::Create(),
          TestParentProcess::CallbacksToSetup::kCleanupRequests,
          &slow_metadata_observer);
  parent_process->AppendSwitchHandleToShare(kSharedEventHandleSwitch,
                                            shared_event->handle());
  parent_process->AppendSwitch(kTestRequestNameSwitch, GetParam());

  // Create test resources that can't be created or cleaned up from the
  // sandbox.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  parent_process->AppendSwitchPath(
      kTestFilePathSwitch, temp_dir.GetPath().Append(L"dummy-file.txt"));

  chrome_cleaner_sandbox::ScopedTempRegistryKey temp_reg_key;
  parent_process->AppendSwitchNative(
      kTestRegKeySwitch,
      base::StrCat({temp_reg_key.FullyQualifiedPath(), L"\\dummy-subkey"}));

  base::Process test_process = LongRunningProcess(/*command_line=*/nullptr);
  parent_process->AppendSwitchNative(
      kTestProcessIdSwitch, base::NumberToString16(test_process.Pid()));

  // Install a test task scheduler so we don't accidentally delete a real task
  // when servicing the DeleteTask request.
  TestTaskScheduler test_task_scheduler;

  int32_t exit_code = -1;
  EXPECT_TRUE(parent_process->LaunchConnectedChildProcess(
      "EngineRequestsNoBlocking", TestTimeouts::action_max_timeout(),
      &exit_code));
  EXPECT_EQ(0, exit_code) << "Child process returned a failure - probably an "
                             "engine request blocked";

  test_process.Terminate(0, true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         EngineRequestsNoBlockingTest,
                         testing::Values("FindFirstFile",
                                         "FindNextFile",
                                         "FindClose",
                                         "OpenReadOnlyFile",
                                         "GetFileAttributes",
                                         "GetKnownFolderPath",
                                         "GetProcesses",
                                         "GetTasks",
                                         "GetProcessImagePath",
                                         "GetLoadedModules",
                                         "GetProcessCommandLine",
                                         "GetUserInfoFromSID",
                                         "OpenReadOnlyRegistry",
                                         "NtOpenReadOnlyRegistry",
#if 0
                                        // Calls using FileRemover still block.
                                        "DeleteFile",
                                        "DeleteFilePostReboot",
#endif
                                         "NtDeleteRegistryKey",
                                         "NtDeleteRegistryValue",
                                         "NtChangeRegistryValue",
                                         "DeleteService",
                                         "DeleteTask",
                                         "TerminateProcess"),
                         GetParamNameForTest());

}  // namespace chrome_cleaner
