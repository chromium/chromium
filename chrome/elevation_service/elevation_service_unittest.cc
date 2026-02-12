// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>

#include <windows.h>

#include <shlobj.h>
#include <wrl/client.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gtest_util.h"
#include "base/test/multiprocess_test.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/win/com_init_util.h"
#include "base/win/elevation_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/security_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/windows_services/service_program/test_support/scoped_medium_integrity.h"
#include "chrome/windows_services/service_program/test_support/service_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr bool kExpectFullIsolation = true;
#else
inline constexpr bool kExpectFullIsolation = false;
#endif

// Relaunches the test process de-elevated; using the multi-process test
// facilitied to run the named function.
base::expected<base::Process, DWORD> RunInChildDeElevated(
    std::string_view function_name) {
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII(switches::kTestChildProcess, function_name);

  return base::win::RunDeElevated(command_line);
}

base::expected<Microsoft::WRL::ComPtr<IElevator2>, HRESULT> GetElevator() {
  base::win::AssertComInitialized();
  Microsoft::WRL::ComPtr<IElevator2> elevator;
  HRESULT hr =
      ::CoCreateInstance(elevation_service::kTestElevatorClsid, nullptr,
                         CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&elevator));

  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to create IElevator2 instance.";
    return base::unexpected(hr);
  }

  hr = ::CoSetProxyBlanket(
      elevator.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to create security blanket.";
    return base::unexpected(hr);
  }

  return elevator;
}

}  // namespace

// A test harness that installs the elevated service so that tests can call it.
// The service's log output is redirected to the test process's stderr.
class ElevationServiceTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (!::IsUserAnAdmin()) {
      GTEST_SKIP() << "Test requires admin rights";
    }
    service_environment_ = new ServiceEnvironment(
        L"Test Elevation Service", FILE_PATH_LITERAL("elevation_service.exe"),
        std::array<std::string_view, 2>{
            elevation_service::switches::kAllowUntrustedPathForTesting,
            elevation_service::switches::kElevatorClsIdForTestingSwitch},
        elevation_service::kTestElevatorClsid, __uuidof(IElevator2));
    ASSERT_TRUE(service_environment_->is_valid());
  }

  static void TearDownTestSuite() {
    delete std::exchange(service_environment_, nullptr);
  }

  // Returns a handle to the service process if it is running, or an invalid
  // process otherwise.
  base::Process GetRunningService() {
    return service_environment_->GetRunningService();
  }

 private:
  static ServiceEnvironment* service_environment_;
};

// static
ServiceEnvironment* ElevationServiceTest::service_environment_ = nullptr;

TEST_F(ElevationServiceTest, AcceptInvitation) {
  base::win::ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  auto elevator = GetElevator();
  ASSERT_TRUE(elevator.has_value());
  auto res = elevator.value()->AcceptInvitation(L"hello");
  EXPECT_EQ(res, E_NOTIMPL);
}

TEST_F(ElevationServiceTest, RunIsolatedChrome) {
  auto child_or_error = RunInChildDeElevated("RunIsolatedChromeInChild");

  if (!child_or_error.has_value()) {
    GTEST_SKIP() << "Cannot de-elevate when UAC is disabled.";
  }
  int exit_code;
  ASSERT_TRUE(child_or_error->WaitForExit(&exit_code));
  ASSERT_EQ(exit_code, 0);
}

// This child process runs at Medium.
MULTIPROCESS_TEST_MAIN(RunIsolatedChromeInChild) {
  base::win::ScopedCOMInitializer com_initializer;
  EXPECT_TRUE(com_initializer.Succeeded());

  const auto event_name = base::Uuid::GenerateRandomV4().AsLowercaseString();
  auto elevator = GetElevator();
  EXPECT_TRUE(elevator.has_value());

  base::CommandLine cmd(base::PathService::CheckedGet(base::DIR_EXE)
                            .Append(L"elevation_service_test_child.exe"));
  // Dangerous switch.
  cmd.AppendSwitch(::switches::kDisableComponentUpdate);
  // Safe switch. The directory used here doesn't matter since it's not going to
  // be used, but allow the test to verify that switch values are passed through
  // correctly.
  cmd.AppendSwitchPath(::switches::kUserDataDir,
                       base::PathService::CheckedGet(base::DIR_EXE));
  cmd.AppendArg(event_name);
  cmd.AppendArg("-invalid-switch");
  cmd.AppendArg("another_arg");

  base::WaitableEvent event(base::win::ScopedHandle(::CreateEventA(
      /*lpEventAttributes=*/nullptr, /*bManualReset=*/FALSE,
      /*bInitialState=*/FALSE, /*lpName=*/std::data(event_name))));

  DWORD last_error = 0;
  ULONG_PTR proc_handle;
  base::win::ScopedBstr log;
  auto res = elevator.value()->RunIsolatedChrome(
      /*flags=*/0, std::data(cmd.GetCommandLineString()), log.Receive(),
      &proc_handle, &last_error);
  EXPECT_HRESULT_SUCCEEDED(res);
  EXPECT_EQ(DWORD{ERROR_SUCCESS}, last_error);
  base::Process process(reinterpret_cast<base::ProcessHandle>(proc_handle));
  if constexpr (kExpectFullIsolation) {
    HANDLE duplicate_proc_handle;
    // Verify not possible to 'upgrade' the handle returned to something with
    // more access.
    EXPECT_FALSE(::DuplicateHandle(
        /*hSourceProcessHandle=*/::GetCurrentProcess(),
        /*hSourceHandle=*/process.Handle(),
        /*hTargetProcessHandle=*/::GetCurrentProcess(),
        /*lpTargetHandle=*/&duplicate_proc_handle,
        /*dwDesiredAccess=*/PROCESS_QUERY_INFORMATION,
        /*bInheritHandle=*/FALSE, /*dwOptions=*/0));
  }

  // If the process has terminated with a validation error then the test will
  // timeout.
  event.Wait();

  EXPECT_TRUE(process.IsRunning());

  // Verify that the child process has correctly been re-parented to the current
  // process.
  EXPECT_EQ(base::Process::Current().Pid(),
            base::GetParentProcessId(process.Handle()));
  const auto pid = process.Pid();
  {
    // Can always open the process read-only.
    const auto read_only_process = base::Process::OpenWithAccess(
        pid, PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION);
    EXPECT_TRUE(read_only_process.IsValid());
  }
  // Any attempt to open isolated process above PROCESS_TERMINATE, SYNCHRONIZE,
  // PROCESS_QUERY_LIMITED_INFORMATION will fail.
  {
    const auto writeable_process = base::Process::Open(pid);
    EXPECT_EQ(kExpectFullIsolation, !writeable_process.IsValid());
  }
  {
    const auto extra_priv_process = base::Process::OpenWithExtraPrivileges(pid);
    EXPECT_EQ(kExpectFullIsolation, !extra_priv_process.IsValid());
  }
  {
    const auto maximum_allowed_process =
        base::Process::OpenWithAccess(pid, MAXIMUM_ALLOWED);
    EXPECT_TRUE(maximum_allowed_process.IsValid());
    const auto access =
        base::win::GetGrantedAccess(maximum_allowed_process.Handle());
    EXPECT_TRUE(access.has_value());
    const DWORD safe_masks[] = {PROCESS_TERMINATE,
                                PROCESS_QUERY_LIMITED_INFORMATION, SYNCHRONIZE};
    const DWORD unsafe_masks[] = {
        PROCESS_CREATE_PROCESS,    PROCESS_CREATE_THREAD,   PROCESS_DUP_HANDLE,
        PROCESS_QUERY_INFORMATION, PROCESS_SET_INFORMATION, PROCESS_SET_QUOTA,
        PROCESS_SUSPEND_RESUME,    PROCESS_VM_OPERATION,    PROCESS_VM_READ,
        PROCESS_VM_WRITE};
    for (const auto safe_mask : safe_masks) {
      EXPECT_TRUE(*access & safe_mask);
    }
    for (const auto unsafe_mask : unsafe_masks) {
      EXPECT_EQ(kExpectFullIsolation, !(*access & unsafe_mask));
    }
  }
  EXPECT_TRUE(process.Terminate(0, /*wait=*/true));

  return ::testing::Test::HasFailure() ? 1 : 0;
}

TEST_F(ElevationServiceTest, Failure) {
  base::win::ScopedCOMInitializer com_initializer;
  EXPECT_TRUE(com_initializer.Succeeded());

  auto elevator = GetElevator();
  EXPECT_TRUE(elevator.has_value());

  DWORD last_error = 0;
  ULONG_PTR proc_handle;
  base::win::ScopedBstr log;
  auto res = elevator.value()->RunIsolatedChrome(
      /*flags=*/0, L"invalid_process_name.exe", log.Receive(), &proc_handle,
      &last_error);
  EXPECT_HRESULT_FAILED(res);
  EXPECT_EQ(DWORD{ERROR_FILE_NOT_FOUND}, last_error);
}
