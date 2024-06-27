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
#include "base/unguessable_token.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_service.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
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

class MockEnterpriseCompanionService final : public EnterpriseCompanionService {
 public:
  MockEnterpriseCompanionService() = default;
  ~MockEnterpriseCompanionService() override = default;

  MOCK_METHOD(void, Shutdown, (base::OnceClosure callback), (override));
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
  int WaitForProcess(base::Process& process, bool should_exit) {
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
    EXPECT_EQ(process_exited, should_exit);
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
  EXPECT_EQ(WaitForProcess(child_process, /*should_exit=*/true),
            static_cast<int>(mojom::EnterpriseCompanion::Result::kSuccess));
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
  WaitForProcess(child_process, /*should_exit=*/false);
}

// A test client which connects to the NamedMojoIpcServer, calls the Shutdown
// RPC, and returns with the result code of the call.
MULTIPROCESS_TEST_MAIN(EnterpriseCompanionClient) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support;

  std::unique_ptr<mojo::IsolatedConnection> connection =
      std::make_unique<mojo::IsolatedConnection>();
  mojo::PlatformChannelEndpoint endpoint =
      named_mojo_ipc_server::ConnectToServer(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
              kServerNameFlag));
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Cannot connect to server: invalid endpoint.";
    return 1;
  }
  mojo::Remote<mojom::EnterpriseCompanion> remote(
      mojo::PendingRemote<mojom::EnterpriseCompanion>(
          connection->Connect(std::move(endpoint)), /*version=*/0));

  base::RunLoop wait_for_response_run_loop;
  int32_t result_code = -1;
  remote->Shutdown(
      base::BindLambdaForTesting(
          [&result_code](mojom::EnterpriseCompanion::Result result) {
            result_code = static_cast<int>(result);
          })
          .Then(wait_for_response_run_loop.QuitClosure()));
  wait_for_response_run_loop.Run(FROM_HERE);

  return result_code;
}

}  // namespace enterprise_companion
