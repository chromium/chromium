// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_ipc_server.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_test_util.h"
#include "components/named_mojo_ipc_server/testing.test-mojom.h"
#include "components/test/test_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace named_mojo_ipc_server {
namespace {

using testing::_;
using testing::Return;

using EchoStringHandler = base::RepeatingCallback<void(
    const std::string& input,
    test::mojom::Echo::EchoStringCallback callback)>;

static const char kEchoClientName[] = "EchoClient";
static const char kClientProcessServerNameSwitch[] =
    "named-mojo-ipc-server-test-server-name";
static const char kClientProcessExitAfterConnectSwitch[] =
    "named-mojo-ipc-server-test-exit-after-connect";
static const char kClientProcessHangAfterConnectSwitch[] =
    "named-mojo-ipc-server-test-hang-after-connect";
static const char kClientProcessWaitUntilDisconnectedSwitch[] =
    "named-mojo-ipc-server-test-wait-until-disconnected";
static const char kClientProcessMessagePipeTypeSwitch[] =
    "named-mojo-ipc-server-test-message-type";
static constexpr int kInvalidEndpointExitCode = 42;
static constexpr uint64_t kTestMessagePipeId = 0u;
static constexpr char kTestMessagePipeName[] = "test-message-pipe-name";

enum class MessagePipeType { ISOLATED, USE_ID, USE_NAME };

std::string MessagePipeTypeToString(MessagePipeType type) {
  switch (type) {
    case MessagePipeType::ISOLATED:
      return "Isolated";
    case MessagePipeType::USE_ID:
      return "MessagePipeId";
    case MessagePipeType::USE_NAME:
      return "MessagePipeName";
  }
}

MessagePipeType MessagePipeTypeFromString(std::string_view type) {
  if (type == "Isolated") {
    return MessagePipeType::ISOLATED;
  }
  if (type == "MessagePipeId") {
    return MessagePipeType::USE_ID;
  }
  if (type == "MessagePipeName") {
    return MessagePipeType::USE_NAME;
  }
  NOTREACHED() << "Unexpected message pipe type: " << type;
}

class NamedMojoIpcServerTest : public testing::TestWithParam<MessagePipeType>,
                               public test::mojom::Echo {
 public:
  NamedMojoIpcServerTest();
  ~NamedMojoIpcServerTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  void CreateIpcServer();
  base::Process LaunchClientProcess(std::string_view extra_switch = {});
  int WaitForProcessExit(base::Process& process);
  void WaitForServerEndpointCreated();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  // Helper thread to wait for process exit without blocking the main thread.
  base::Thread wait_for_process_exit_thread_{"wait_for_process_exit"};

  std::unique_ptr<NamedMojoIpcServer<test::mojom::Echo>> ipc_server_;

  mojo::ReceiverId last_echo_string_receiver_id_ = 0u;
  base::ProcessId last_echo_string_peer_pid_ = base::kNullProcessId;

  // If this is set, EchoString() will run this callback instead of responding
  // to the callback automatically.
  EchoStringHandler echo_string_handler_;

  base::RepeatingClosure on_echo_string_called_;

 private:
  void EchoString(const std::string& input,
                  EchoStringCallback callback) override;

  void OnServerEndpointCreated();

  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  mojo::NamedPlatformChannel::ServerName test_server_name_;

  // Run loops that wait for NamedMojoIpcServerBase::ObserverForTesting methods
  // to be called.
  std::unique_ptr<base::RunLoop> on_server_endpoint_created_run_loop_;
};

NamedMojoIpcServerTest::NamedMojoIpcServerTest() {
  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      task_environment_.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  wait_for_process_exit_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  test_server_name_ = test::GenerateRandomServerName();
  CreateIpcServer();
  on_server_endpoint_created_run_loop_ = std::make_unique<base::RunLoop>();
}

NamedMojoIpcServerTest::~NamedMojoIpcServerTest() = default;

void NamedMojoIpcServerTest::SetUp() {
  ipc_server_->StartServer();
  WaitForServerEndpointCreated();
}

void NamedMojoIpcServerTest::TearDown() {
  if (ipc_server_) {
    ipc_server_->StopServer();
  }
  task_environment_.RunUntilIdle();
}

void NamedMojoIpcServerTest::CreateIpcServer() {
  EndpointOptions options;
  options.server_name = test_server_name_;
  switch (GetParam()) {
    case MessagePipeType::ISOLATED:
      break;
    case MessagePipeType::USE_ID:
      options.message_pipe_id = kTestMessagePipeId;
      break;
    case MessagePipeType::USE_NAME:
      options.message_pipe_id = kTestMessagePipeName;
      break;
  }
  ipc_server_ = std::make_unique<NamedMojoIpcServer<test::mojom::Echo>>(
      options, base::BindRepeating([](test::mojom::Echo* impl,
                                      const ConnectionInfo&) { return impl; },
                                   this));
  ipc_server_->set_on_server_endpoint_created_callback_for_testing(
      base::BindRepeating(&NamedMojoIpcServerTest::OnServerEndpointCreated,
                          base::Unretained(this)));
}

void NamedMojoIpcServerTest::WaitForServerEndpointCreated() {
  on_server_endpoint_created_run_loop_->Run();
  on_server_endpoint_created_run_loop_ = std::make_unique<base::RunLoop>();
}

base::Process NamedMojoIpcServerTest::LaunchClientProcess(
    std::string_view extra_switch) {
  base::CommandLine cmd_line = base::GetMultiProcessTestChildBaseCommandLine();
  cmd_line.AppendSwitchNative(kClientProcessServerNameSwitch,
                              test_server_name_);
  if (!extra_switch.empty()) {
    cmd_line.AppendSwitch(extra_switch);
  }
  cmd_line.AppendSwitchASCII(kClientProcessMessagePipeTypeSwitch,
                             MessagePipeTypeToString(GetParam()));
  if (GetParam() == MessagePipeType::ISOLATED) {
    // Make sure the new process is a broker, because isolated connections are
    // only supported between two brokers when ipcz is enabled.
    if (mojo::core::IsMojoIpczEnabled()) {
      cmd_line.AppendSwitch(switches::kInitializeMojoAsBroker);
    }
  }
  return base::SpawnMultiProcessTestChild(kEchoClientName, cmd_line,
                                          /* options= */ {});
}

int NamedMojoIpcServerTest::WaitForProcessExit(base::Process& process) {
  int exit_code;
  bool process_exited = false;
  base::RunLoop wait_for_process_exit_loop;
  wait_for_process_exit_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        process_exited = base::WaitForMultiprocessTestChildExit(
            process, TestTimeouts::action_timeout(), &exit_code);
      }),
      wait_for_process_exit_loop.QuitClosure());
  wait_for_process_exit_loop.Run();
  process.Close();
  EXPECT_TRUE(process_exited);
  return exit_code;
}

void NamedMojoIpcServerTest::EchoString(const std::string& input,
                                        EchoStringCallback callback) {
  if (echo_string_handler_) {
    echo_string_handler_.Run(input, std::move(callback));
    return;
  }

  std::move(callback).Run(input);
  last_echo_string_receiver_id_ = ipc_server_->current_receiver();
  last_echo_string_peer_pid_ = ipc_server_->current_connection_info().pid;

  if (on_echo_string_called_) {
    on_echo_string_called_.Run();
  }
}

void NamedMojoIpcServerTest::OnServerEndpointCreated() {
  on_server_endpoint_created_run_loop_->Quit();
}

TEST_P(NamedMojoIpcServerTest, SendEcho) {
  base::Process child_process = LaunchClientProcess();
  base::ProcessId child_pid = child_process.Pid();
  EXPECT_EQ(0, WaitForProcessExit(child_process));
  EXPECT_EQ(child_pid, last_echo_string_peer_pid_);
}

TEST_P(NamedMojoIpcServerTest, DeleteNamedMojoServer_NoLingeringInvitations) {
  ipc_server_.reset();
  base::RunLoop().RunUntilIdle();
  // For posix, the socket doesn't seem to be closed immediately after the
  // connection is deleted, so we wait for 1s to make sure the socket is really
  // closed.
  base::PlatformThread::Sleep(base::Seconds(1));

  base::Process child_process = LaunchClientProcess();
  EXPECT_EQ(kInvalidEndpointExitCode, WaitForProcessExit(child_process));
}

TEST_P(NamedMojoIpcServerTest, DisconnectHandler) {
  base::RunLoop disconnect_run_loop;
  base::MockCallback<base::RepeatingClosure> disconnect_handler;
  EXPECT_CALL(disconnect_handler, Run()).WillOnce([&]() {
    ASSERT_EQ(last_echo_string_receiver_id_, ipc_server_->current_receiver());
    disconnect_run_loop.Quit();
  });
  ipc_server_->set_disconnect_handler(disconnect_handler.Get());

  base::Process child_process = LaunchClientProcess();
  WaitForProcessExit(child_process);

  disconnect_run_loop.Run();
}

TEST_P(NamedMojoIpcServerTest, DeleteNamedMojoServer_RemoteDisconnected) {
  base::RunLoop wait_for_echo_string_called_run_loop;
  on_echo_string_called_ = wait_for_echo_string_called_run_loop.QuitClosure();
  base::Process child_process =
      LaunchClientProcess(kClientProcessWaitUntilDisconnectedSwitch);
  wait_for_echo_string_called_run_loop.Run();

  ipc_server_.reset();
  WaitForProcessExit(child_process);
}

TEST_P(NamedMojoIpcServerTest, StopServer_RemoteDisconnected) {
  base::RunLoop wait_for_echo_string_called_run_loop;
  on_echo_string_called_ = wait_for_echo_string_called_run_loop.QuitClosure();
  base::Process child_process =
      LaunchClientProcess(kClientProcessWaitUntilDisconnectedSwitch);
  wait_for_echo_string_called_run_loop.Run();

  ipc_server_->StopServer();
  WaitForProcessExit(child_process);
}

TEST_P(NamedMojoIpcServerTest, CloseReceiver_RemoteDisconnected) {
  base::RunLoop wait_for_echo_string_called_run_loop;
  on_echo_string_called_ = wait_for_echo_string_called_run_loop.QuitClosure();
  base::Process child_process =
      LaunchClientProcess(kClientProcessWaitUntilDisconnectedSwitch);
  wait_for_echo_string_called_run_loop.Run();
  ASSERT_EQ(1u, ipc_server_->GetNumberOfActiveConnectionsForTesting());

  ipc_server_->Close(last_echo_string_receiver_id_);
  WaitForProcessExit(child_process);
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
}

TEST_P(NamedMojoIpcServerTest, CloseNonexistentReceiver_NoCrash) {
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
  ipc_server_->Close(1u);
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
}

TEST_P(NamedMojoIpcServerTest, RemoteProcessTerminated_ConnectionRemoved) {
  base::RunLoop wait_for_echo_string_called_run_loop;
  on_echo_string_called_ = wait_for_echo_string_called_run_loop.QuitClosure();
  base::Process child_process =
      LaunchClientProcess(kClientProcessWaitUntilDisconnectedSwitch);
  wait_for_echo_string_called_run_loop.Run();
  ASSERT_EQ(1u, ipc_server_->GetNumberOfActiveConnectionsForTesting());

  base::RunLoop disconnect_run_loop;
  ipc_server_->set_disconnect_handler(disconnect_run_loop.QuitClosure());
  base::TerminateMultiProcessTestChild(child_process, 0, true);
  disconnect_run_loop.Run();
  ASSERT_EQ(0u, ipc_server_->GetNumberOfActiveConnectionsForTesting());
}

// On Windows the server endpoint must be recreated between connections. The
// following tests check this behavior.
#if BUILDFLAG(IS_WIN)
TEST_P(NamedMojoIpcServerTest,
       RemoteTerminatedBeforeBound_NewServerEndpointCreated) {
  base::Process child_process =
      LaunchClientProcess(kClientProcessExitAfterConnectSwitch);
  WaitForProcessExit(child_process);
  WaitForServerEndpointCreated();
}

TEST_P(NamedMojoIpcServerTest,
       RemoteConnectsAndHangs_NewServerEndpointCreated) {
  base::Process child_process =
      LaunchClientProcess(kClientProcessHangAfterConnectSwitch);
  WaitForServerEndpointCreated();
}
#endif

TEST_P(NamedMojoIpcServerTest, ParallelIpcs) {
  base::MockCallback<EchoStringHandler> mock_echo_string_handler;
  echo_string_handler_ = mock_echo_string_handler.Get();
  std::string first_input;
  EchoStringCallback first_callback;
  EXPECT_CALL(mock_echo_string_handler, Run(_, _))
      .WillOnce([&](const std::string& input, EchoStringCallback callback) {
        first_input = input;
        first_callback = std::move(callback);
      })
      .WillOnce([&](const std::string& input, EchoStringCallback callback) {
        std::move(first_callback).Run(first_input);
        std::move(callback).Run(input);
      });

  base::Process child_process_1 = LaunchClientProcess();
#if BUILDFLAG(IS_WIN)
  // Wait for the named pipe to be recreated. Otherwise, the next client
  // connection races this event.
  WaitForServerEndpointCreated();
#endif
  base::Process child_process_2 = LaunchClientProcess();

  WaitForProcessExit(child_process_1);
  WaitForProcessExit(child_process_2);
}

TEST_P(NamedMojoIpcServerTest, IpcServerRestarted_NewIpcsCanBeMade) {
  base::Process child_process = LaunchClientProcess();
  WaitForProcessExit(child_process);

  ipc_server_.reset();
  CreateIpcServer();
  ipc_server_->StartServer();
  WaitForServerEndpointCreated();

  child_process = LaunchClientProcess();
  WaitForProcessExit(child_process);
}

// Client process main function. The default behavior is to send "test string"
// to the EchoString() interface and verify that the server returns the same
// string.
MULTIPROCESS_TEST_MAIN(EchoClient) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  auto ipc_support = std::make_unique<mojo::core::ScopedIPCSupport>(
      task_environment.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  auto server_name =
      cmd_line->GetSwitchValueNative(kClientProcessServerNameSwitch);
  EXPECT_FALSE(server_name.empty());
  mojo::PlatformChannelEndpoint endpoint = ConnectToServer(server_name);
  if (!endpoint.is_valid()) {
    return kInvalidEndpointExitCode;
  }
  if (cmd_line->HasSwitch(kClientProcessExitAfterConnectSwitch)) {
    return 0;
  }
  if (cmd_line->HasSwitch(kClientProcessHangAfterConnectSwitch)) {
    base::RunLoop().Run();
    return 0;
  }
  std::unique_ptr<mojo::IsolatedConnection> connection;
  mojo::ScopedMessagePipeHandle message_pipe;
  MessagePipeType message_pipe_type = MessagePipeTypeFromString(
      cmd_line->GetSwitchValueASCII(kClientProcessMessagePipeTypeSwitch));
  if (message_pipe_type == MessagePipeType::ISOLATED) {
    connection = std::make_unique<mojo::IsolatedConnection>();
    message_pipe = connection->Connect(std::move(endpoint));
  } else {
    auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
    if (message_pipe_type == MessagePipeType::USE_ID) {
      message_pipe = invitation.ExtractMessagePipe(kTestMessagePipeId);
    } else {
      EXPECT_EQ(message_pipe_type, MessagePipeType::USE_NAME);
      message_pipe = invitation.ExtractMessagePipe(kTestMessagePipeName);
    }
  }

  auto echo_remote =
      mojo::Remote<test::mojom::Echo>(mojo::PendingRemote<test::mojom::Echo>(
          std::move(message_pipe), /* version= */ 0));
  base::RunLoop wait_for_disconnect_run_loop;
  echo_remote.set_disconnect_handler(
      wait_for_disconnect_run_loop.QuitClosure());

  base::RunLoop echo_response_run_loop;
  auto callback = base::BindLambdaForTesting([&](const std::string& response) {
    ASSERT_EQ("test string", response);
    echo_response_run_loop.Quit();
  });
  echo_remote->EchoString("test string", std::move(callback));

  if (cmd_line->HasSwitch(kClientProcessWaitUntilDisconnectedSwitch)) {
    // The server might close the connection before the response is sent, so we
    // don't wait for the echo response here.
    wait_for_disconnect_run_loop.Run();
  } else {
    echo_response_run_loop.Run();
  }
  return 0;
}

INSTANTIATE_TEST_SUITE_P(
    /* test_prefix */,
    NamedMojoIpcServerTest,
    testing::Values(true, false),
    [](const testing::TestParamInfo<MessagePipeType>& info) {
      return MessagePipeTypeToString(info.param);
    });

}  // namespace
}  // namespace named_mojo_ipc_server
