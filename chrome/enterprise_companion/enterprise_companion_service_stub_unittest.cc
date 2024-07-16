// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_service_stub.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_clock.h"
#include "base/unguessable_token.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_service.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom-forward.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace enterprise_companion {
namespace {

constexpr char kServerNameFlag[] = "server-name";

// Exit code emitted by the multi-process test client if an IPC call is dropped.
constexpr int32_t kClientCallbackDroppedExitCode = 42;

class MockEnterpriseCompanionService final : public EnterpriseCompanionService {
 public:
  MockEnterpriseCompanionService() = default;
  ~MockEnterpriseCompanionService() override = default;

  MOCK_METHOD(void, Shutdown, (base::OnceClosure callback), (override));
  MOCK_METHOD(void, FetchPolicies, (StatusCallback callback), (override));
};

}  // namespace

class EnterpriseCompanionServiceStubTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_service_ = std::make_unique<MockEnterpriseCompanionService>();
  }

  void TearDown() override {
    // NamedMojoIpcServer requires test environments to run until idle to avoid
    // a memory leak.
    environment_.RunUntilIdle();
  }

 protected:
  base::Process SpawnClient() {
    base::CommandLine command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    command_line.AppendSwitchNative(kServerNameFlag, server_name_);
    return base::SpawnMultiProcessTestChild("EnterpriseCompanionClient",
                                            command_line,
                                            /*options=*/{});
  }

  // Waits for a process to exit or appear stuck. The process exit code is
  // returned if exited.
  int WaitForProcess(base::Process& process) {
    int exit_code = 0;
    bool process_exited = false;
    base::RunLoop wait_for_process_exit_loop;
    wait_for_process_exit_runner_->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
          process_exited = base::WaitForMultiprocessTestChildExit(
              process, TestTimeouts::action_timeout() / 2, &exit_code);
        }),
        wait_for_process_exit_loop.QuitClosure());
    wait_for_process_exit_loop.Run();
    process.Close();
    EXPECT_TRUE(process_exited);
    return exit_code;
  }

  mojo::NamedPlatformChannel::ServerName server_name_ = GetTestServerName();
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support_;
  // Helper thread to wait for process exit without blocking the main thread.
  scoped_refptr<base::SequencedTaskRunner> wait_for_process_exit_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  std::unique_ptr<MockEnterpriseCompanionService> mock_service_;

 private:
  static mojo::NamedPlatformChannel::ServerName GetTestServerName() {
#if BUILDFLAG(IS_MAC)
    return base::StrCat({"org.chromium.ChromeEnterpriseCompanionTest",
                         base::UnguessableToken::Create().ToString(),
                         ".service"});
#elif BUILDFLAG(IS_LINUX)
    return base::GetTempDirForTesting()
        .AppendASCII(base::StrCat({"ChromeEnterpriseCompanionTest",
                                   base::UnguessableToken::Create().ToString(),
                                   ".service.sk"}))
        .AsUTF8Unsafe();
#elif BUILDFLAG(IS_WIN)
    return base::UTF8ToWide(
        base::StrCat({"org.chromium.ChromeEnterpriseCompanionTest",
                      base::UnguessableToken::Create().ToString()}));
#endif
  }
};

TEST_F(EnterpriseCompanionServiceStubTest, ServiceReachable) {
  EXPECT_CALL(*mock_service_, Shutdown)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  // Start the companion service and wait for it to become available before
  // launching the child process.
  base::RunLoop start_run_loop;
  std::unique_ptr<mojom::EnterpriseCompanion> stub =
      CreateEnterpriseCompanionServiceStub(
          std::move(mock_service_), {.server_name = server_name_},
          base::BindRepeating([](const named_mojo_ipc_server::ConnectionInfo&) {
            return true;
          }),
          start_run_loop.QuitClosure());
  start_run_loop.Run(FROM_HERE);

  base::Process child_process = SpawnClient();
  EXPECT_EQ(WaitForProcess(child_process), 0);
}

TEST_F(EnterpriseCompanionServiceStubTest, UntrustedCallerRejected) {
  EXPECT_CALL(*mock_service_, Shutdown).Times(0);
  base::RunLoop start_run_loop;
  std::unique_ptr<mojom::EnterpriseCompanion> stub =
      CreateEnterpriseCompanionServiceStub(
          std::move(mock_service_), {.server_name = server_name_},
          base::BindRepeating([](const named_mojo_ipc_server::ConnectionInfo&) {
            return false;
          }),
          start_run_loop.QuitClosure());
  start_run_loop.Run(FROM_HERE);

  base::Process child_process = SpawnClient();
  EXPECT_EQ(WaitForProcess(child_process), kClientCallbackDroppedExitCode);
}

// A test client which connects to the NamedMojoIpcServer, calls the Shutdown
// RPC, and returns with the result code of the call.
MULTIPROCESS_TEST_MAIN(EnterpriseCompanionClient) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support;

  std::unique_ptr<mojo::IsolatedConnection> connection;
  mojo::Remote<mojom::EnterpriseCompanion> remote;
  base::RunLoop connect_run_loop;
  ConnectToServer(
      base::DefaultClock::GetInstance(), TestTimeouts::action_timeout(),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<mojo::IsolatedConnection> in_connection,
              mojo::Remote<mojom::EnterpriseCompanion> in_remote) {
            connection = std::move(in_connection);
            remote = std::move(in_remote);
          })
          .Then(connect_run_loop.QuitClosure()),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kServerNameFlag));
  connect_run_loop.Run();

  if (!connection || !remote) {
    LOG(ERROR) << "Failed to connect to remote";
    return -1;
  }

  int32_t result_code = -1;
  base::RunLoop wait_for_response_run_loop;
  remote->Shutdown(
      base::BindOnce(
          [](int32_t* out_result_code, mojom::StatusPtr status) {
            *out_result_code = status->code;
          },
          &result_code)
          .Then(mojo::WrapCallbackWithDropHandler(
              mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                  base::BindOnce(wait_for_response_run_loop.QuitClosure())),
              base::BindLambdaForTesting(
                  [&]() { result_code = kClientCallbackDroppedExitCode; }))));
  wait_for_response_run_loop.Run();

  return result_code;
}

}  // namespace enterprise_companion
