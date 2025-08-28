// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>

#include <windows.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <string_view>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected.h"
#include "base/win/elevation_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "chrome/windows_services/elevated_tracing_service/system_tracing_session.h"
#include "chrome/windows_services/elevated_tracing_service/tracing_service_idl.h"
#include "chrome/windows_services/elevated_tracing_service/with_child_test.h"
#include "chrome/windows_services/service_program/test_support/scoped_medium_integrity.h"
#include "chrome/windows_services/service_program/test_support/service_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

void TestGetServiceProcessHandle() {
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(
      elevated_tracing_service::kTestSystemTracingSessionClsid, nullptr,
      CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)));

  Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session;
  ASSERT_HRESULT_SUCCEEDED(unknown.As(&trace_session));
  unknown.Reset();

  ASSERT_HRESULT_SUCCEEDED(::CoSetProxyBlanket(
      trace_session.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING));

  mojo::NamedPlatformChannel channel(mojo::NamedPlatformChannel::Options{});
  mojo::OutgoingInvitation invitation;
  invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(/*name=*/0);

  DWORD pid = base::kNullProcessId;
  ASSERT_HRESULT_SUCCEEDED(
      trace_session->AcceptInvitation(channel.GetServerName().c_str(), &pid));
  ASSERT_NE(pid, base::kNullProcessId);

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 /*target_process=*/base::kNullProcessHandle,
                                 channel.TakeServerEndpoint());

  // Wait for the service to connect to the pipe.
  mojo::SimpleWatcher pipe_watcher(FROM_HERE,
                                   mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  base::RunLoop run_loop;
  ASSERT_EQ(
      pipe_watcher.Watch(
          pipe.get(),
          MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          base::BindLambdaForTesting(
              [quit_closure = run_loop.QuitClosure()](
                  MojoResult result, const mojo::HandleSignalsState& state) {
                EXPECT_EQ(result, MOJO_RESULT_OK);
                EXPECT_EQ(state.satisfied_signals, MOJO_HANDLE_SIGNAL_WRITABLE);
                quit_closure.Run();
              })),
      MOJO_RESULT_OK);
  pipe_watcher.ArmOrNotify();
  run_loop.Run();

  // Releasing `trace_session` will trigger termination in the service.
}

// Tests that AcceptInvitation refuses a second invitation from a client.
void TestOnlyOneInvitation() {
  // Create a session and activate it by sending an invitation.
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(
      elevated_tracing_service::kTestSystemTracingSessionClsid, nullptr,
      CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)));

  Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session;
  ASSERT_HRESULT_SUCCEEDED(unknown.As(&trace_session));
  unknown.Reset();

  ASSERT_HRESULT_SUCCEEDED(::CoSetProxyBlanket(
      trace_session.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING));

  mojo::ScopedMessagePipeHandle pipe;
  {
    mojo::NamedPlatformChannel channel(mojo::NamedPlatformChannel::Options{});
    mojo::OutgoingInvitation invitation;
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
    pipe = invitation.AttachMessagePipe(/*name=*/0);

    DWORD pid = base::kNullProcessId;
    ASSERT_HRESULT_SUCCEEDED(
        trace_session->AcceptInvitation(channel.GetServerName().c_str(), &pid));
    ASSERT_NE(pid, base::kNullProcessId);

    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   /*target_process=*/base::kNullProcessHandle,
                                   channel.TakeServerEndpoint());
  }

  {
    mojo::NamedPlatformChannel channel(mojo::NamedPlatformChannel::Options{});
    mojo::OutgoingInvitation invitation;
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
    mojo::ScopedMessagePipeHandle pipe2 =
        invitation.AttachMessagePipe(/*name=*/0);

    DWORD pid = base::kNullProcessId;
    ASSERT_EQ(
        trace_session->AcceptInvitation(channel.GetServerName().c_str(), &pid),
        kErrorSessionAlreadyActive);
  }
}

// Tests that a second session is refused while a first is active.
void TestOnlyOneSession() {
  // Create a session and activate it by sending an invitation.
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(
      elevated_tracing_service::kTestSystemTracingSessionClsid, nullptr,
      CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)));

  Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session;
  ASSERT_HRESULT_SUCCEEDED(unknown.As(&trace_session));
  unknown.Reset();

  ASSERT_HRESULT_SUCCEEDED(::CoSetProxyBlanket(
      trace_session.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING));

  mojo::ScopedMessagePipeHandle pipe;
  {
    mojo::NamedPlatformChannel channel(mojo::NamedPlatformChannel::Options{});
    mojo::OutgoingInvitation invitation;
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
    pipe = invitation.AttachMessagePipe(/*name=*/0);

    DWORD pid = base::kNullProcessId;
    ASSERT_HRESULT_SUCCEEDED(
        trace_session->AcceptInvitation(channel.GetServerName().c_str(), &pid));
    ASSERT_NE(pid, base::kNullProcessId);

    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   /*target_process=*/base::kNullProcessHandle,
                                   channel.TakeServerEndpoint());
  }

  // Now a second client is refused.
  ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(
      elevated_tracing_service::kTestSystemTracingSessionClsid, nullptr,
      CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown)));

  Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session2;
  ASSERT_HRESULT_SUCCEEDED(unknown.As(&trace_session2));
  unknown.Reset();

  ASSERT_HRESULT_SUCCEEDED(::CoSetProxyBlanket(
      trace_session2.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING));

  mojo::NamedPlatformChannel channel(mojo::NamedPlatformChannel::Options{});
  mojo::OutgoingInvitation invitation;
  invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
  mojo::ScopedMessagePipeHandle pipe2 =
      invitation.AttachMessagePipe(/*name=*/0);

  DWORD pid = base::kNullProcessId;
  ASSERT_EQ(
      trace_session2->AcceptInvitation(channel.GetServerName().c_str(), &pid),
      kErrorSessionInProgress);
}

MULTIPROCESS_TEST_MAIN(GetServiceProcessHandleInChild) {
  // base::debug::WaitForDebugger(30, /*silent=*/false);

  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded()) {
    return 1;
  }

  base::test::SingleThreadTaskEnvironment task_environment;

  TestGetServiceProcessHandle();

  return ::testing::Test::HasFailure() ? 1 : 0;
}

MULTIPROCESS_TEST_MAIN(OnlyOneInvitationInChild) {
  // base::debug::WaitForDebugger(30, /*silent=*/false);

  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded()) {
    return 1;
  }

  TestOnlyOneInvitation();

  return ::testing::Test::HasFailure() ? 1 : 0;
}

MULTIPROCESS_TEST_MAIN(OnlyOneSessionInChild) {
  // base::debug::WaitForDebugger(30, /*silent=*/false);

  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded()) {
    return 1;
  }

  TestOnlyOneSession();

  return ::testing::Test::HasFailure() ? 1 : 0;
}

// Relaunches the test process de-elevated; using the multi-process test
// facilitied to run the named function.
base::expected<base::Process, DWORD> RunInChildDeElevated(
    std::string_view function_name) {
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII(switches::kTestChildProcess, function_name);

  return base::win::RunDeElevated(command_line);
}

}  // namespace

// A test harness that installs the tracing service so that tests can call into
// it. The service's log output is redirected to the test process's stderr.
class TracingServiceTest : public elevated_tracing_service::WithChildTest {
 protected:
  static void SetUpTestSuite() {
    if (!::IsUserAnAdmin()) {
      GTEST_SKIP() << "Test requires admin rights";
    }
    service_environment_ = new ServiceEnvironment(
        L"Test Tracing Service",
        FILE_PATH_LITERAL("elevated_tracing_service.exe"),
        elevated_tracing_service::switches::kSystemTracingClsIdForTestingSwitch,
        elevated_tracing_service::kTestSystemTracingSessionClsid,
        __uuidof(ISystemTraceSession));
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
ServiceEnvironment* TracingServiceTest::service_environment_ = nullptr;

// Tests calling into the service from the main test process itself after
// dropping to medium integrity level.
TEST_F(TracingServiceTest, GetServiceProcessHandle) {
  base::win::ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  ScopedMediumIntegrity medium_integrity;
  ASSERT_TRUE(medium_integrity.Succeeded());

  TestGetServiceProcessHandle();
}

// Tests calling into the service from the main test process itself after
// dropping to medium integrity level.
TEST_F(TracingServiceTest, OnlyOneInvitation) {
  base::win::ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  ScopedMediumIntegrity medium_integrity;
  ASSERT_TRUE(medium_integrity.Succeeded());

  TestOnlyOneInvitation();
}

// Tests calling into the service from the main test process itself after
// dropping to medium integrity level.
TEST_F(TracingServiceTest, OnlyOneSession) {
  base::win::ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  ScopedMediumIntegrity medium_integrity;
  ASSERT_TRUE(medium_integrity.Succeeded());

  TestOnlyOneSession();
}

// Tests calling into the service from a de-elevated child process. This test is
// skipped if UAC is disabled.
TEST_F(TracingServiceTest, GetServiceProcessHandleInChild) {
  // Pro tip: Add --show-error-dialogs to the test's command line and put
  // base::debug::WaitForDebugger(30, /*silent=*/false); in
  // `GetServiceProcessHandleInChild()` to more easily debug in the child
  // process.
  auto child_or_error = RunInChildDeElevated("GetServiceProcessHandleInChild");

  if (!child_or_error.has_value()) {
    GTEST_SKIP() << "Cannot de-elevate when UAC is disabled.";
  }
  int exit_code;
  ASSERT_TRUE(child_or_error->WaitForExit(&exit_code));
  ASSERT_EQ(exit_code, 0);
}

// Tests calling into the service from a de-elevated child process. This test is
// skipped if UAC is disabled.
TEST_F(TracingServiceTest, OnlyOneInvitationInChild) {
  // Pro tip: Add --show-error-dialogs to the test's command line and put
  // base::debug::WaitForDebugger(30, /*silent=*/false); in
  // `GetServiceProcessHandleInChild()` to more easily debug in the child
  // process.
  auto child_or_error = RunInChildDeElevated("OnlyOneInvitationInChild");

  if (!child_or_error.has_value()) {
    GTEST_SKIP() << "Cannot de-elevate when UAC is disabled.";
  }
  int exit_code;
  ASSERT_TRUE(child_or_error->WaitForExit(&exit_code));
  ASSERT_EQ(exit_code, 0);
}

// Tests calling into the service from a de-elevated child process. This test is
// skipped if UAC is disabled.
TEST_F(TracingServiceTest, OnlyOneSessionInChild) {
  // Pro tip: Add --show-error-dialogs to the test's command line and put
  // base::debug::WaitForDebugger(30, /*silent=*/false); in
  // `GetServiceProcessHandleInChild()` to more easily debug in the child
  // process.
  auto child_or_error = RunInChildDeElevated("OnlyOneSessionInChild");

  if (!child_or_error.has_value()) {
    GTEST_SKIP() << "Cannot de-elevate when UAC is disabled.";
  }
  int exit_code;
  ASSERT_TRUE(child_or_error->WaitForExit(&exit_code));
  ASSERT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ConnectToServiceAndTerminateInChild) {
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded()) {
    return 1;
  }

  // Create a session and activate it by sending an invitation.
  Microsoft::WRL::ComPtr<IUnknown> unknown;
  CHECK(SUCCEEDED(::CoCreateInstance(
      elevated_tracing_service::kTestSystemTracingSessionClsid, nullptr,
      CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&unknown))));

  Microsoft::WRL::ComPtr<ISystemTraceSession> trace_session;
  CHECK(SUCCEEDED(unknown.As(&trace_session)));
  unknown.Reset();

  CHECK(SUCCEEDED(::CoSetProxyBlanket(
      trace_session.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
      COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING)));

  mojo::ScopedMessagePipeHandle pipe;
  {
    mojo::NamedPlatformChannel channel(mojo::NamedPlatformChannel::Options{});
    mojo::OutgoingInvitation invitation;
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_ELEVATED);
    pipe = invitation.AttachMessagePipe(/*name=*/0);

    DWORD pid = base::kNullProcessId;
    CHECK(SUCCEEDED(trace_session->AcceptInvitation(
        channel.GetServerName().c_str(), &pid)));
    CHECK_NE(pid, base::kNullProcessId);

    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   /*target_process=*/base::kNullProcessHandle,
                                   channel.TakeServerEndpoint());
  }

  // A session is now active. Tell the test that the child has started. The test
  // will monitor the service and then release the child.
  elevated_tracing_service::WithChildTest::SignalChildStart();

  // Wait for the test to release the child for termination.
  elevated_tracing_service::WithChildTest::WaitForChildTermination();

  base::Process::TerminateCurrentProcessImmediately(1);
}

// Tests that the service terminates promptly when the child process terminates
// while holding a session.
TEST_F(TracingServiceTest, ServiceTerminatesOnChildTermination) {
  auto child =
      SpawnChildWithEventHandles("ConnectToServiceAndTerminateInChild");
  ASSERT_TRUE(child.IsValid());
  absl::Cleanup child_stopper = [this] { SignalChildTermination(); };

  // Wait for the child to attach to the service.
  WaitForChildStart();

  // Get a handle to the service process.
  auto service_process = GetRunningService();
  ASSERT_TRUE(service_process.IsValid());

  // Tell the child to terminate.
  std::move(child_stopper).Invoke();

  // Wait for the service to terminate.
  ASSERT_EQ(::WaitForSingleObject(
                service_process.Handle(),
                TestTimeouts::action_max_timeout().InMilliseconds()),
            WAIT_OBJECT_0);
}
