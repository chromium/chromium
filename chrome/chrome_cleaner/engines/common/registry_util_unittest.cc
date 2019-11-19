// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/common/registry_util.h"

#include <windows.h>
#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/win_util.h"
#include "chrome/chrome_cleaner/ipc/ipc_test_util.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/windows_handle.mojom.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/test/test_native_reg_util.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "sandbox/win/src/win_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using base::WaitableEvent;
using chrome_cleaner::MojoTaskRunner;
using chrome_cleaner::String16EmbeddedNulls;
using chrome_cleaner::mojom::TestWindowsHandle;

namespace chrome_cleaner_sandbox {

namespace {

class TestFile : public base::File {
 public:
  TestFile()
      : base::File(
            chrome_cleaner::PreFetchedPaths::GetInstance()->GetExecutablePath(),
            FLAG_OPEN) {}
};

class TestWindowsHandleImpl : public TestWindowsHandle {
 public:
  explicit TestWindowsHandleImpl(
      mojo::PendingReceiver<TestWindowsHandle> receiver)
      : receiver_(this, std::move(receiver)) {}

  // TestWindowsHandle

  void EchoHandle(HANDLE handle, EchoHandleCallback callback) override {
    std::move(callback).Run(handle);
  }

  void EchoRawHandle(mojo::ScopedHandle handle,
                     EchoRawHandleCallback callback) override {
    std::move(callback).Run(std::move(handle));
  }

 private:
  mojo::Receiver<TestWindowsHandle> receiver_;
};

class SandboxParentProcess : public chrome_cleaner::ParentProcess {
 public:
  explicit SandboxParentProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ParentProcess(std::move(mojo_task_runner)) {}

 protected:
  void CreateImpl(mojo::ScopedMessagePipeHandle mojo_pipe) override {
    mojo::PendingReceiver<TestWindowsHandle> receiver(std::move(mojo_pipe));
    test_windows_handle_impl_ =
        std::make_unique<TestWindowsHandleImpl>(std::move(receiver));
  }

  void DestroyImpl() override { test_windows_handle_impl_.reset(); }

 private:
  ~SandboxParentProcess() override = default;

  std::unique_ptr<TestWindowsHandleImpl> test_windows_handle_impl_;
};

class SandboxChildProcess : public chrome_cleaner::ChildProcess {
 public:
  explicit SandboxChildProcess(scoped_refptr<MojoTaskRunner> mojo_task_runner)
      : ChildProcess(mojo_task_runner),
        test_windows_handle_(
            std::make_unique<mojo::Remote<TestWindowsHandle>>()) {}

  void BindToPipe(mojo::ScopedMessagePipeHandle mojo_pipe,
                  WaitableEvent* event) {
    test_windows_handle_->Bind(
        mojo::PendingRemote<TestWindowsHandle>(std::move(mojo_pipe), 0));
    event->Signal();
  }

  HANDLE EchoHandle(HANDLE input_handle) {
    HANDLE output_handle;
    WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                        WaitableEvent::InitialState::NOT_SIGNALED);
    auto callback = base::BindOnce(
        [](HANDLE* handle_holder, WaitableEvent* event, HANDLE handle) {
          *handle_holder = handle;
          event->Signal();
        },
        &output_handle, &event);
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::Remote<TestWindowsHandle>* remote, HANDLE handle,
               TestWindowsHandle::EchoHandleCallback callback) {
              (*remote)->EchoHandle(std::move(handle), std::move(callback));
            },
            base::Unretained(test_windows_handle_.get()), input_handle,
            std::move(callback)));
    event.Wait();
    return output_handle;
  }

  HANDLE EchoRawHandle(HANDLE input_handle) {
    mojo::ScopedHandle scoped_handle = mojo::WrapPlatformFile(input_handle);
    mojo::ScopedHandle output_handle;
    WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                        WaitableEvent::InitialState::NOT_SIGNALED);
    auto callback = base::BindOnce(
        [](mojo::ScopedHandle* handle_holder, WaitableEvent* event,
           mojo::ScopedHandle handle) {
          *handle_holder = std::move(handle);
          event->Signal();
        },
        &output_handle, &event);

    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](mojo::Remote<TestWindowsHandle>* remote,
                          mojo::ScopedHandle handle,
                          TestWindowsHandle::EchoRawHandleCallback callback) {
                         (*remote)->EchoRawHandle(std::move(handle),
                                                  std::move(callback));
                       },
                       base::Unretained(test_windows_handle_.get()),
                       base::Passed(&scoped_handle), std::move(callback)));
    event.Wait();

    HANDLE raw_output_handle;
    CHECK_EQ(
        mojo::UnwrapPlatformFile(std::move(output_handle), &raw_output_handle),
        MOJO_RESULT_OK);
    return raw_output_handle;
  }

 private:
  ~SandboxChildProcess() override {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<mojo::Remote<TestWindowsHandle>> remote) {
              remote.reset();
            },
            base::Passed(&test_windows_handle_)));
  }

  std::unique_ptr<mojo::Remote<TestWindowsHandle>> test_windows_handle_;
};

base::string16 HandlePath(HANDLE handle) {
  base::string16 full_path;
  // The size parameter of GetFinalPathNameByHandle does NOT include the null
  // terminator.
  DWORD result = ::GetFinalPathNameByHandleW(
      handle, base::WriteInto(&full_path, MAX_PATH), MAX_PATH - 1, 0);
  if (result > MAX_PATH) {
    result = ::GetFinalPathNameByHandle(
        handle, base::WriteInto(&full_path, result), result - 1, 0);
  }
  if (!result) {
    PLOG(ERROR) << "Could not get full path for handle " << handle;
    return base::string16();
  }
  return full_path;
}

::testing::AssertionResult HandlesAreEqual(HANDLE handle1, HANDLE handle2) {
  // The best way to check this is CompareObjectHandles, but it isn't available
  // until Windows 10. So just check that both refer to the same path.
  base::string16 path1 = HandlePath(handle1);
  base::string16 path2 = HandlePath(handle2);

  if (path1.empty() || path2.empty() || path1 != path2) {
    auto format_message = [](HANDLE handle, const base::string16& path) {
      std::ostringstream s;
      s << handle;
      if (path.empty())
        s << " has no valid path";
      else
        s << " has path " << path;
      return s.str();
    };
    return ::testing::AssertionFailure()
           << format_message(handle1, path1) << ", "
           << format_message(handle2, path2);
  }
  return ::testing::AssertionSuccess();
}

}  // namespace

MULTIPROCESS_TEST_MAIN(HandleWrappingIPCMain) {
  auto mojo_task_runner = MojoTaskRunner::Create();
  auto child_process =
      base::MakeRefCounted<SandboxChildProcess>(mojo_task_runner);
  auto message_pipe_handle = child_process->CreateMessagePipeFromCommandLine();

  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  mojo_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&SandboxChildProcess::BindToPipe, child_process,
                                base::Passed(&message_pipe_handle), &event));
  event.Wait();

  // Check that this test is actually testing what it thinks it is: when
  // passing a mojo::ScopedHandle to another process, Mojo attempts to
  // duplicate and close the original handle. This fails if the ScopedHandle is
  // wrapping a Windows pseudo-handle, and nullptr gets passed instead. The
  // WindowsHandle wrapper avoids this.
  //
  // However, if the mojo::ScopedHandle is passed over a Mojo connection that
  // isn't going to another process, pseudo-handles are passed correctly. So
  // make sure the test connection actually causes the error with
  // mojo::ScopedHandle. If not, the tests of WindowsHandle will trivially
  // succeed without demonstrating that WindowsHandle avoids the error.
  CHECK_EQ(nullptr, child_process->EchoRawHandle(HKEY_CLASSES_ROOT));

  CHECK_EQ(INVALID_HANDLE_VALUE,
           child_process->EchoHandle(INVALID_HANDLE_VALUE));
  CHECK_EQ(nullptr, child_process->EchoHandle(nullptr));
  CHECK_EQ(HKEY_CLASSES_ROOT, child_process->EchoHandle(HKEY_CLASSES_ROOT));
  CHECK_EQ(HKEY_CURRENT_CONFIG, child_process->EchoHandle(HKEY_CURRENT_CONFIG));
  CHECK_EQ(HKEY_CURRENT_USER, child_process->EchoHandle(HKEY_CURRENT_USER));
  CHECK_EQ(HKEY_LOCAL_MACHINE, child_process->EchoHandle(HKEY_LOCAL_MACHINE));
  CHECK_EQ(HKEY_USERS, child_process->EchoHandle(HKEY_USERS));

  // mojo::ScopedHandle CHECKS if given an invalid handle, so pass this handle
  // raw and ensure it is marked as invalid.
  HANDLE fake_handle = base::win::Uint32ToHandle(0x12345678);
  CHECK_EQ(nullptr, child_process->EchoRawHandle(fake_handle));

  TestFile test_file;
  HANDLE test_handle = test_file.GetPlatformFile();
  CHECK(HandlesAreEqual(test_handle, child_process->EchoHandle(test_handle)));

  return 0;
}

TEST(SandboxUtil, HandleWrappingIPC) {
  auto mojo_task_runner = MojoTaskRunner::Create();
  auto parent_process =
      base::MakeRefCounted<SandboxParentProcess>(mojo_task_runner);

  int32_t exit_code = -1;
  EXPECT_TRUE(parent_process->LaunchConnectedChildProcess(
      "HandleWrappingIPCMain", &exit_code));
  EXPECT_EQ(0, exit_code);
}

TEST(SandboxUtil, NativeQueryValueKey) {
  std::vector<wchar_t> key_name{L'a', L'b', L'c', L'\0'};
  String16EmbeddedNulls value_name1{L'f', L'o', L'o', L'1', L'\0'};
  String16EmbeddedNulls value_name2{L'f', L'o', L'o', L'2', L'\0'};
  String16EmbeddedNulls value_name3{L'f', L'o', L'o', L'3', L'\0'};
  String16EmbeddedNulls value_name4{L'f', L'o', L'o', L'4', L'\0'};
  String16EmbeddedNulls value{L'b', L'a', L'r', L'\0'};

  struct TestCases {
    const String16EmbeddedNulls& value_name;
    ULONG reg_type;
  } test_cases[] = {
      {value_name1, REG_SZ},
      {value_name2, REG_EXPAND_SZ},
      {value_name3, REG_DWORD},
      {value_name4, REG_BINARY},
  };

  ScopedTempRegistryKey temp_key;

  ULONG disposition = 0;
  HANDLE subkey_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(STATUS_SUCCESS, NativeCreateKey(temp_key.Get(), &key_name,
                                            &subkey_handle, &disposition));
  EXPECT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);

  for (auto test_case : test_cases) {
    EXPECT_EQ(STATUS_SUCCESS,
              NativeSetValueKey(subkey_handle, test_case.value_name,
                                test_case.reg_type, value));
    DWORD actual_reg_type = 0;
    String16EmbeddedNulls actual_value;
    EXPECT_TRUE(NativeQueryValueKey(subkey_handle, test_case.value_name,
                                    &actual_reg_type, &actual_value));
    EXPECT_EQ(test_case.reg_type, actual_reg_type);
    EXPECT_EQ(value, actual_value);
  }

  EXPECT_EQ(STATUS_SUCCESS, NativeDeleteKey(subkey_handle));
  EXPECT_TRUE(::CloseHandle(subkey_handle));
}

TEST(SandboxUtil, ValidateRegistryValueChange) {
  String16EmbeddedNulls eq{L'a', L'b', L'\0', L'c'};
  EXPECT_TRUE(ValidateRegistryValueChange(eq, eq));

  String16EmbeddedNulls subset1{L'a', L'b', L'\0', L'c'};
  String16EmbeddedNulls subset2{L'a', L'\0', L'c'};
  EXPECT_TRUE(ValidateRegistryValueChange(subset1, subset2));

  String16EmbeddedNulls prefix1{L'a', L'b', L'\0', L'c'};
  String16EmbeddedNulls prefix2{L'b', L'\0', L'c'};
  EXPECT_TRUE(ValidateRegistryValueChange(prefix1, prefix2));

  String16EmbeddedNulls suffix1{L'a', L'b', L'\0', L'c'};
  String16EmbeddedNulls suffix2{L'a', L'b', L'\0'};
  EXPECT_TRUE(ValidateRegistryValueChange(suffix1, suffix2));

  String16EmbeddedNulls empty1{L'a', L'b', L'\0', L'c'};
  String16EmbeddedNulls empty2;
  EXPECT_TRUE(ValidateRegistryValueChange(empty1, empty2));

  String16EmbeddedNulls super_empty1;
  String16EmbeddedNulls super_empty2{L'a', L'b', L'\0', L'c'};
  EXPECT_FALSE(ValidateRegistryValueChange(super_empty1, super_empty2));

  String16EmbeddedNulls superset1{L'a', L'\0', L'c'};
  String16EmbeddedNulls superset2{L'a', L'b', L'\0', L'c'};
  EXPECT_FALSE(ValidateRegistryValueChange(superset1, superset2));

  String16EmbeddedNulls bad_prefix1{L'b', L'\0', L'c'};
  String16EmbeddedNulls bad_prefix2{L'a', L'b', L'\0', L'c'};
  EXPECT_FALSE(ValidateRegistryValueChange(bad_prefix1, bad_prefix2));

  String16EmbeddedNulls bad_suffix1{L'a', L'b', L'\0'};
  String16EmbeddedNulls bad_suffix2{L'a', L'b', L'\0', L'c'};
  EXPECT_FALSE(ValidateRegistryValueChange(bad_suffix1, bad_suffix2));

  String16EmbeddedNulls different1{L'a', L'b', L'\0', L'c'};
  String16EmbeddedNulls different2{L'd', L'e', L'f'};
  EXPECT_FALSE(ValidateRegistryValueChange(different1, different2));
}

TEST(SandboxUtil, ValidateFailureOfNtSetValueKeyOnNull) {
  // The documentation for NtSetValueKey is incorrect, calling it with a
  // NULL pointer returns access denied. This test ensures that the observed
  // behaviour remains consistent.
  std::vector<wchar_t> key_name{L'a', L'b', L'c', L'\0'};
  std::vector<wchar_t> value{L'b', L'a', L'r', L'\0'};

  ScopedTempRegistryKey temp_key;

  ULONG disposition = 0;
  HANDLE subkey_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(STATUS_SUCCESS, NativeCreateKey(temp_key.Get(), &key_name,
                                            &subkey_handle, &disposition));
  EXPECT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);

  static NtSetValueKeyFunction NtSetValueKey = nullptr;
  if (!NtSetValueKey)
    ResolveNTFunctionPtr("NtSetValueKey", &NtSetValueKey);

  NTSTATUS status =
      NtSetValueKey(subkey_handle, nullptr, /*TitleIndex=*/0, REG_SZ,
                    reinterpret_cast<void*>(value.data()),
                    base::checked_cast<ULONG>(value.size() * sizeof(wchar_t)));
  EXPECT_EQ(static_cast<NTSTATUS>(STATUS_ACCESS_VIOLATION), status);

  EXPECT_EQ(STATUS_SUCCESS, NativeDeleteKey(subkey_handle));
  EXPECT_TRUE(::CloseHandle(subkey_handle));
}

TEST(SandboxUtil, ValidateFailureOfNtQueryValueKeyOnNull) {
  // The documentation for NtQueryValueKey is incorrect, calling it with a
  // NULL pointer returns access denied. This test ensures that the observed
  // behaviour remains consistent.
  std::vector<wchar_t> key_name{L'a', L'b', L'c', L'\0'};

  String16EmbeddedNulls value{L'b', L'a', L'r', L'\0'};

  ScopedTempRegistryKey temp_key;

  ULONG disposition = 0;
  HANDLE subkey_handle = INVALID_HANDLE_VALUE;
  EXPECT_EQ(STATUS_SUCCESS, NativeCreateKey(temp_key.Get(), &key_name,
                                            &subkey_handle, &disposition));
  EXPECT_EQ(static_cast<ULONG>(REG_CREATED_NEW_KEY), disposition);

  // Create a default value.
  EXPECT_EQ(STATUS_SUCCESS,
            NativeSetValueKey(subkey_handle, String16EmbeddedNulls(nullptr),
                              REG_SZ, value));

  static NtQueryValueKeyFunction NtQueryValueKey = nullptr;
  if (!NtQueryValueKey)
    ResolveNTFunctionPtr("NtQueryValueKey", &NtQueryValueKey);

  DWORD size_needed = 0;
  NTSTATUS status =
      NtQueryValueKey(subkey_handle, nullptr, KeyValueFullInformation, nullptr,
                      0, &size_needed);
  EXPECT_EQ(static_cast<NTSTATUS>(STATUS_ACCESS_VIOLATION), status);

  EXPECT_EQ(STATUS_SUCCESS, NativeDeleteKey(subkey_handle));
  EXPECT_TRUE(::CloseHandle(subkey_handle));
}

}  // namespace chrome_cleaner_sandbox
