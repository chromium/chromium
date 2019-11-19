// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_service.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_queue.h"
#include "components/policy/core/common/remote_commands/test_remote_command_job.h"
#include "components/policy/core/common/remote_commands/testing_remote_commands_server.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ReturnNew;

namespace policy {

namespace {

namespace em = enterprise_management;

const char kDMToken[] = "dmtoken";
const char kTestPayload[] = "_testing_payload_";
const int kTestCommandExecutionTimeInSeconds = 1;
const int kTestClientServerCommunicationDelayInSeconds = 3;

void ExpectSucceededJob(const std::string& expected_payload,
                        const em::RemoteCommandResult& command_result) {
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            command_result.result());
  EXPECT_EQ(expected_payload, command_result.payload());
}

void ExpectIgnoredJob(const em::RemoteCommandResult& command_result) {
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_IGNORED,
            command_result.result());
}

}  // namespace

// Mocked RemoteCommand factory to allow us to build test commands.
class MockTestRemoteCommandFactory : public RemoteCommandsFactory {
 public:
  MockTestRemoteCommandFactory() {
    ON_CALL(*this, BuildTestCommand())
        .WillByDefault(ReturnNew<TestRemoteCommandJob>(
            true,
            base::TimeDelta::FromSeconds(kTestCommandExecutionTimeInSeconds)));
  }

  MOCK_METHOD0(BuildTestCommand, TestRemoteCommandJob*());

 private:
  // RemoteCommandJobsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      em::RemoteCommand_Type type,
      RemoteCommandsService* service) override {
    if (type != em::RemoteCommand_Type_COMMAND_ECHO_TEST) {
      ADD_FAILURE();
      return nullptr;
    }
    return base::WrapUnique<RemoteCommandJob>(BuildTestCommand());
  }

  DISALLOW_COPY_AND_ASSIGN(MockTestRemoteCommandFactory);
};

// Expectations for a single FetchRemoteCommands() call.
struct FetchCallExpectation {
  FetchCallExpectation() = default;

  FetchCallExpectation SetCommandResults(size_t n) {
    expected_command_results = n;
    return *this;
  }
  FetchCallExpectation SetFetchedCommands(size_t n) {
    expected_fetched_commands = n;
    return *this;
  }
  FetchCallExpectation SetSignedCommands(size_t n) {
    expected_signed_commands = n;
    return *this;
  }
  FetchCallExpectation SetFetchedCallback(base::Closure callback) {
    commands_fetched_callback = callback;
    return *this;
  }

  size_t expected_command_results = 0;
  size_t expected_fetched_commands = 0;
  size_t expected_signed_commands = 0;
  base::Closure commands_fetched_callback = base::DoNothing();
};

// A mocked CloudPolicyClient to interact with a TestingRemoteCommandsServer.
class TestingCloudPolicyClientForRemoteCommands : public CloudPolicyClient {
 public:
  explicit TestingCloudPolicyClientForRemoteCommands(
      TestingRemoteCommandsServer* server)
      : CloudPolicyClient(std::string() /* machine_id */,
                          std::string() /* machine_model */,
                          std::string() /* brand_code */,
                          std::string() /* ethernet_mac_address */,
                          std::string() /* dock_mac_address */,
                          std::string() /* manufacture_date */,
                          nullptr /* service */,
                          nullptr /* url_loader_factory */,
                          nullptr /* signing_service */,
                          CloudPolicyClient::DeviceDMTokenCallback()),
        server_(server) {
    dm_token_ = kDMToken;
  }

  ~TestingCloudPolicyClientForRemoteCommands() override {
    EXPECT_TRUE(expected_fetch_commands_calls_.empty());
  }

  // Expect a FetchRemoteCommands() call with |fetch_call_expectation|.
  void ExpectFetchCommands(FetchCallExpectation fetch_call_expectation) {
    expected_fetch_commands_calls_.push(fetch_call_expectation);
  }

 private:
  void FetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<em::RemoteCommandResult>& command_results,
      RemoteCommandCallback callback) override {
    ASSERT_FALSE(expected_fetch_commands_calls_.empty());

    const FetchCallExpectation fetch_call_expectation =
        expected_fetch_commands_calls_.front();
    expected_fetch_commands_calls_.pop();

    // Simulate delay from client to DMServer.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &TestingCloudPolicyClientForRemoteCommands::DoFetchRemoteCommands,
            base::Unretained(this), std::move(last_command_id), command_results,
            std::move(callback), fetch_call_expectation),
        base::TimeDelta::FromSeconds(
            kTestClientServerCommunicationDelayInSeconds));
  }

  void DoFetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<em::RemoteCommandResult>& command_results,
      RemoteCommandCallback callback,
      const FetchCallExpectation& fetch_call_expectation) {
    std::vector<em::RemoteCommand> fetched_commands;
    std::vector<em::SignedData> signed_commands;
    server_->FetchCommands(std::move(last_command_id), command_results,
                           &fetched_commands, &signed_commands);

    // The server will send us either old-style unsigned or new signed commands,
    // never both at the same time.
    EXPECT_TRUE(fetched_commands.size() == 0 || signed_commands.size() == 0);

    EXPECT_EQ(fetch_call_expectation.expected_command_results,
              command_results.size());
    EXPECT_EQ(fetch_call_expectation.expected_fetched_commands,
              fetched_commands.size());
    EXPECT_EQ(fetch_call_expectation.expected_signed_commands,
              signed_commands.size());

    if (!fetch_call_expectation.commands_fetched_callback.is_null())
      fetch_call_expectation.commands_fetched_callback.Run();

    // Simulate delay from DMServer back to client.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), DM_STATUS_SUCCESS, fetched_commands,
                       signed_commands),
        base::TimeDelta::FromSeconds(
            kTestClientServerCommunicationDelayInSeconds));
  }

  base::queue<FetchCallExpectation> expected_fetch_commands_calls_;
  TestingRemoteCommandsServer* server_;

  DISALLOW_COPY_AND_ASSIGN(TestingCloudPolicyClientForRemoteCommands);
};

// Base class for unit tests regarding remote commands service.
class RemoteCommandsServiceTest : public testing::Test {
 protected:
  RemoteCommandsServiceTest()
      : server_(std::make_unique<TestingRemoteCommandsServer>()) {
    server_->SetClock(mock_task_runner_->GetMockTickClock());
    cloud_policy_client_ =
        std::make_unique<TestingCloudPolicyClientForRemoteCommands>(
            server_.get());
  }

  void StartService(std::unique_ptr<RemoteCommandsFactory> factory) {
    remote_commands_service_ = std::make_unique<RemoteCommandsService>(
        std::move(factory), cloud_policy_client_.get(), &store_);
    remote_commands_service_->SetClocksForTesting(
        mock_task_runner_->GetMockClock(),
        mock_task_runner_->GetMockTickClock());
  }

  void FlushAllTasks() { mock_task_runner_->FastForwardUntilNoTasksRemain(); }

  const scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::TestMockTimeTaskRunner::Type::kBoundToThread);

  std::unique_ptr<TestingRemoteCommandsServer> server_;
  std::unique_ptr<TestingCloudPolicyClientForRemoteCommands>
      cloud_policy_client_;
  MockCloudPolicyStore store_;
  std::unique_ptr<RemoteCommandsService> remote_commands_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCommandsServiceTest);
};

// Tests that no command will be fetched if no commands is issued.
TEST_F(RemoteCommandsServiceTest, NoCommands) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(0);

  StartService(std::move(factory));

  // A fetch requst should get nothing from server.
  cloud_policy_client_->ExpectFetchCommands(FetchCallExpectation());
  EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

  FlushAllTasks();
}

// Tests that existing commands issued before service started will be fetched.
TEST_F(RemoteCommandsServiceTest, ExistingCommand) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(1);

  {
    base::RunLoop run_loop;

    // Issue a command before service started.
    server_->IssueCommand(em::RemoteCommand_Type_COMMAND_ECHO_TEST,
                          kTestPayload,
                          base::Bind(&ExpectSucceededJob, kTestPayload), false);

    // Start the service, run until the command is fetched.
    cloud_policy_client_->ExpectFetchCommands(
        FetchCallExpectation().SetFetchedCommands(1).SetFetchedCallback(
            run_loop.QuitClosure()));
    StartService(std::move(factory));
    EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

    run_loop.Run();
  }

  // And run again so that the result can be reported.
  cloud_policy_client_->ExpectFetchCommands(
      FetchCallExpectation().SetCommandResults(1));

  FlushAllTasks();

  EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());
}

// Tests that commands issued after service started will be fetched.
TEST_F(RemoteCommandsServiceTest, NewCommand) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(1);

  StartService(std::move(factory));

  // Set up expectations on fetch commands calls. The first request will fetch
  // one command, and the second will fetch none but provide result for the
  // previous command instead.
  cloud_policy_client_->ExpectFetchCommands(
      FetchCallExpectation().SetFetchedCommands(1));
  cloud_policy_client_->ExpectFetchCommands(
      FetchCallExpectation().SetCommandResults(1));

  // Issue a command and manually start a command fetch.
  server_->IssueCommand(em::RemoteCommand_Type_COMMAND_ECHO_TEST, kTestPayload,
                        base::Bind(&ExpectSucceededJob, kTestPayload), false);
  EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

  FlushAllTasks();

  EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());
}

// Tests that commands issued after service started will be fetched, even if
// the command is issued when a fetch request is ongoing.
TEST_F(RemoteCommandsServiceTest, NewCommandFollwingFetch) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(1);

  StartService(std::move(factory));

  {
    base::RunLoop run_loop;

    // Add a command which will be issued after first fetch.
    server_->IssueCommand(em::RemoteCommand_Type_COMMAND_ECHO_TEST,
                          kTestPayload,
                          base::Bind(&ExpectSucceededJob, kTestPayload), true);

    cloud_policy_client_->ExpectFetchCommands(
        FetchCallExpectation().SetFetchedCallback(run_loop.QuitClosure()));

    // Attempts to fetch commands.
    EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

    // There should be no issued command at this point.
    EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());

    // The command fetch should be in progress.
    EXPECT_TRUE(remote_commands_service_->IsCommandFetchInProgressForTesting());

    // And a following up fetch request should be enqueued.
    EXPECT_FALSE(remote_commands_service_->FetchRemoteCommands());

    // Run until first fetch request is completed.
    run_loop.Run();
  }

  // The command should be issued now. Note that this command was actually
  // issued before the first fetch request completes in previous run loop.
  EXPECT_EQ(1u, server_->NumberOfCommandsPendingResult());

  cloud_policy_client_->ExpectFetchCommands(
      FetchCallExpectation().SetFetchedCommands(1));
  cloud_policy_client_->ExpectFetchCommands(
      FetchCallExpectation().SetCommandResults(1));

  // No further fetch request is made, but the new issued command should be
  // fetched and executed.
  FlushAllTasks();

  EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());
}

// Tests that on_command_acked_callback_ gets called after the commands get
// acked/fetched (one function handles both).
TEST_F(RemoteCommandsServiceTest, AckedCallback) {
  std::unique_ptr<MockTestRemoteCommandFactory> factory(
      new MockTestRemoteCommandFactory());
  EXPECT_CALL(*factory, BuildTestCommand()).Times(1);

  StartService(std::move(factory));

  bool on_command_acked_callback_called = false;
  remote_commands_service_->SetOnCommandAckedCallback(base::BindOnce(
      [](bool* on_command_acked_callback_called) {
        *on_command_acked_callback_called = true;
      },
      &on_command_acked_callback_called));

  // Set up expectations on fetch commands calls. The first request will fetch
  // one command, and the second will fetch none but provide result for the
  // previous command instead.
  cloud_policy_client_->ExpectFetchCommands(
      FetchCallExpectation().SetFetchedCommands(1));
  cloud_policy_client_->ExpectFetchCommands(
      FetchCallExpectation().SetCommandResults(1));

  // Issue a command and manually start a command fetch.
  server_->IssueCommand(em::RemoteCommand_Type_COMMAND_ECHO_TEST, kTestPayload,
                        base::Bind(&ExpectSucceededJob, kTestPayload), false);
  EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());

  FlushAllTasks();

  EXPECT_TRUE(on_command_acked_callback_called);
}

class EnsureCalled {
 public:
  EnsureCalled() = default;
  ~EnsureCalled() { CHECK(called_times_ == 1); }

  void Bind(ResultReportedCallback callback) {
    callback_ = std::move(callback);
  }

  void Call(const em::RemoteCommandResult& command_result) {
    called_times_++;
    std::move(callback_).Run(command_result);
  }

 private:
  int called_times_ = 0;
  ResultReportedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(EnsureCalled);
};

class SignedRemoteCommandsServiceTest : public RemoteCommandsServiceTest {
 protected:
  SignedRemoteCommandsServiceTest() {
    StartService(std::make_unique<MockTestRemoteCommandFactory>());

    // Set the public key and the device id.
    std::vector<uint8_t> public_key = PolicyBuilder::GetPublicTestKey();
    store_.policy_signature_public_key_.assign(public_key.begin(),
                                               public_key.end());
    store_.policy_ = std::make_unique<em::PolicyData>();
    store_.policy_->set_device_id("acme-device");

    // Set up expectations on fetch commands calls. The first request will fetch
    // one secure command, and the second will fetch none but provide result for
    // the previous command instead.
    cloud_policy_client_->ExpectFetchCommands(
        FetchCallExpectation().SetSignedCommands(1));
    cloud_policy_client_->ExpectFetchCommands(
        FetchCallExpectation().SetCommandResults(1));
  }

  ~SignedRemoteCommandsServiceTest() override {
    EXPECT_TRUE(remote_commands_service_->FetchRemoteCommands());
    FlushAllTasks();
    EXPECT_EQ(0u, server_->NumberOfCommandsPendingResult());
  }

  EnsureCalled ensure_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignedRemoteCommandsServiceTest);
};

// Tests that signed remote commands work.
TEST_F(SignedRemoteCommandsServiceTest, Success) {
  ensure_called_.Bind(base::BindOnce(&ExpectSucceededJob, std::string()));
  server_->IssueSignedCommand(
      base::BindOnce(&EnsureCalled::Call, base::Unretained(&ensure_called_)),
      nullptr, nullptr, nullptr);
}

// Tests that we reject signed remote commands with invalid signature.
TEST_F(SignedRemoteCommandsServiceTest, InvalidSignature) {
  em::SignedData signed_data;
  signed_data.set_data("some-random-data");
  signed_data.set_signature("random-signature");

  ensure_called_.Bind(base::BindOnce(&ExpectIgnoredJob));
  server_->IssueSignedCommand(
      base::BindOnce(&EnsureCalled::Call, base::Unretained(&ensure_called_)),
      nullptr, nullptr, &signed_data);
}

// Tests that we reject signed remote commands with invalid PolicyData type.
TEST_F(SignedRemoteCommandsServiceTest, InvalidPolicyDataType) {
  em::PolicyData policy_data;
  policy_data.set_policy_type("some-random-policy-type");

  ensure_called_.Bind(base::BindOnce(&ExpectIgnoredJob));
  server_->IssueSignedCommand(
      base::BindOnce(&EnsureCalled::Call, base::Unretained(&ensure_called_)),
      nullptr, &policy_data, nullptr);
}

// Tests that we reject signed remote commands with invalid RemoteCommand data.
TEST_F(SignedRemoteCommandsServiceTest, InvalidRemoteCommand) {
  em::PolicyData policy_data;
  policy_data.set_policy_type("google/chromeos/remotecommand");

  ensure_called_.Bind(base::BindOnce(&ExpectIgnoredJob));
  server_->IssueSignedCommand(
      base::BindOnce(&EnsureCalled::Call, base::Unretained(&ensure_called_)),
      nullptr, &policy_data, nullptr);
}

// Tests that we reject signed remote commands with invalid target device id.
TEST_F(SignedRemoteCommandsServiceTest, InvalidDeviceId) {
  em::RemoteCommand remote_command;
  remote_command.set_target_device_id("roadrunner-device");

  ensure_called_.Bind(base::BindOnce(&ExpectIgnoredJob));
  server_->IssueSignedCommand(
      base::BindOnce(&EnsureCalled::Call, base::Unretained(&ensure_called_)),
      &remote_command, nullptr, nullptr);
}

}  // namespace policy
