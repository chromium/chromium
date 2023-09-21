// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_service.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/test_support/echo_remote_command_job.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/core/common/remote_commands/test_support/testing_remote_commands_server.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "remote_commands_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnNew;
using testing::StrictMock;

namespace policy {

namespace {

namespace em = enterprise_management;

const char kDMToken[] = "dmtoken";
const int kTestEchoCommandExecutionTimeInSeconds = 1;
const char kDeviceId[] = "acme-device";

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0)

// Future value that waits for the remote command result that is sent to the
// server.
class ServerResponseFuture
    : public base::test::TestFuture<em::RemoteCommandResult> {
 public:
  ServerResponseFuture() = default;
  ServerResponseFuture(const ServerResponseFuture&) = delete;
  ServerResponseFuture& operator=(const ServerResponseFuture&) = delete;
  ~ServerResponseFuture() = default;

  ResultReportedCallback GetCallback() {
    return base::test::TestFuture<em::RemoteCommandResult>::GetCallback<
        const em::RemoteCommandResult&>();
  }
};

// A fake remote command job. It will not report completion until the test
// calls any of the Finish*() methods.
class FakeJob : public RemoteCommandJob {
 public:
  explicit FakeJob(enterprise_management::RemoteCommand_Type type)
      : type_(type) {}
  FakeJob(const FakeJob&) = delete;
  FakeJob& operator=(const FakeJob&) = delete;
  ~FakeJob() override = default;

  // Returns the payload passed to |ParseCommandPayload|.
  const std::string& GetPayload() const { return payload_; }

  // Finish this job and report success.
  void FinishWithSuccess(const std::string& payload) {
    DCHECK(!result_callback_.is_null());
    std::move(result_callback_).Run(ResultType::kSuccess, payload);
  }
  // Finish this job and report an error.
  void FinishWithFailure(const std::string& payload) {
    DCHECK(!result_callback_.is_null());
    std::move(result_callback_).Run(ResultType::kFailure, payload);
  }

  void Finish() { FinishWithSuccess(""); }

  // RemoteCommandJob implementation:
  enterprise_management::RemoteCommand_Type GetType() const override {
    return type_;
  }
  bool ParseCommandPayload(const std::string& command_payload) override {
    payload_ = command_payload;
    return true;
  }
  bool IsExpired(base::TimeTicks now) override { return false; }
  void RunImpl(CallbackWithResult result_callback) override {
    result_callback_ = std::move(result_callback);
  }

 private:
  const enterprise_management::RemoteCommand_Type type_;

  // The payload passed to ParseCommandPayload().
  std::string payload_;

  CallbackWithResult result_callback_;
};

// Fake RemoteCommand factory that creates FakeJob instances.
// It also provides a WaitForJob() method that will block until a job is
// created, and return the corresponding fake_job. It is the responsibility of
// the test to finish the fake_job (by calling any of the Finish*() methods).
class FakeJobFactory : public RemoteCommandsFactory {
 public:
  FakeJobFactory() = default;
  FakeJobFactory(const FakeJobFactory&) = delete;
  FakeJobFactory& operator=(const FakeJobFactory&) = delete;
  ~FakeJobFactory() override { DCHECK(created_jobs_.IsEmpty()); }

  // Wait until a new job is constructed.
  FakeJob& WaitForJob() {
    FakeJob* result = created_jobs_.Take();
    DCHECK_NE(result, nullptr);
    return *result;
  }

 private:
  // RemoteCommandJobsFactory:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      em::RemoteCommand_Type type,
      RemoteCommandsService* service) override {
    auto result = std::make_unique<FakeJob>(type);

    created_jobs_.AddValue(result.get());
    return result;
  }

  // List of all jobs created in BuildJobForType() and consumed in
  // WaitForJob().
  base::test::RepeatingTestFuture<FakeJob*> created_jobs_;
};

// Mocked RemoteCommand factory.
// This will by default create remote command jobs that finish automatically,
// but you can overwrite this by adding an EXPECT_CALL to your code:
// EXPECT_CALL(job_factory, BuildJobForType).WillOnce(Return(<your job>));
//
// You can use this instead of |FakeJobFactory| if:
//     - You need to simulate a failure to create the job (in which case you
//       can return nullptr like this):
//         EXPECT_CALL(job_factory, BuildJobForType).WillOnce(Return(nullptr));
//     - You want the job to auto-finish without having to call one of the
//       Finish*() methods like you have to do with the |FakeJobFactory|.
class MockJobFactory : public RemoteCommandsFactory {
 public:
  MockJobFactory() {
    ON_CALL(*this, BuildJobForType(testing::_))
        .WillByDefault(ReturnNew<EchoRemoteCommandJob>(
            /*succeed=*/true,
            /*execution_duration=*/base::Seconds(
                kTestEchoCommandExecutionTimeInSeconds)));
  }
  MockJobFactory(const MockJobFactory&) = delete;
  MockJobFactory& operator=(const MockJobFactory&) = delete;

  MOCK_METHOD(RemoteCommandJob*,
              BuildJobForType,
              (em::RemoteCommand::Type type));

  // ON_CALL(...).WillByDefault() does not support methods that return an
  // unique_ptr, so we have to mock a version that returns a raw pointer, and
  // wrap it here.
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      em::RemoteCommand::Type type,
      RemoteCommandsService* service) override {
    return base::WrapUnique<RemoteCommandJob>(BuildJobForType(type));
  }
};

// A mocked CloudPolicyClient to interact with a TestingRemoteCommandsServer.
class TestingCloudPolicyClientForRemoteCommands : public CloudPolicyClient {
 public:
  explicit TestingCloudPolicyClientForRemoteCommands(
      TestingRemoteCommandsServer* server,
      PolicyInvalidationScope scope)
      : CloudPolicyClient(nullptr /* service */,
                          nullptr /* url_loader_factory */,
                          CloudPolicyClient::DeviceDMTokenCallback()),
        server_(server),
        scope_(scope) {
    dm_token_ = kDMToken;
  }
  TestingCloudPolicyClientForRemoteCommands(
      const TestingCloudPolicyClientForRemoteCommands&) = delete;
  TestingCloudPolicyClientForRemoteCommands& operator=(
      const TestingCloudPolicyClientForRemoteCommands&) = delete;

  ~TestingCloudPolicyClientForRemoteCommands() override = default;

 private:
  void FetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<em::RemoteCommandResult>& command_results,
      em::PolicyFetchRequest::SignatureType signature_type,
      const std::string& request_type,
      RemoteCommandCallback callback) override {
    std::vector<em::SignedData> commands =
        server_->FetchCommands(std::move(last_command_id), command_results);

    EXPECT_EQ(RemoteCommandsService::GetRequestType(scope_), request_type);

    // Asynchronously send the response from the DMServer back to client.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), DM_STATUS_SUCCESS, commands));
  }

  raw_ptr<TestingRemoteCommandsServer> server_;
  PolicyInvalidationScope scope_;
};

}  // namespace

// Base class for unit tests regarding remote commands service.
class RemoteCommandsServiceTest
    : public testing::TestWithParam<PolicyInvalidationScope> {
 public:
  RemoteCommandsServiceTest(const RemoteCommandsServiceTest&) = delete;
  RemoteCommandsServiceTest& operator=(const RemoteCommandsServiceTest&) =
      delete;

 protected:
  void SetUp() override {
    // Set the public key and the device id.
    std::vector<uint8_t> public_key = PolicyBuilder::GetPublicTestKey();
    store_.policy_signature_public_key_.assign(public_key.begin(),
                                               public_key.end());
    auto policy_data = std::make_unique<em::PolicyData>();
    policy_data->set_device_id("acme-device");
    store_.set_policy_data_for_testing(std::move(policy_data));
  }

  RemoteCommandsServiceTest() {
    server_.SetClock(mock_task_runner_->GetMockTickClock());
  }

  // Starts the RemoteCommandService using a job factory of the given type.
  // Returns a reference to the job factory.
  template <typename FactoryType>
  FactoryType& StartServiceWith() {
    auto factory = std::make_unique<FactoryType>();
    auto* factory_ptr = factory.get();

    remote_commands_service_ = std::make_unique<RemoteCommandsService>(
        std::move(factory), &cloud_policy_client_, &store_, GetScope());
    remote_commands_service_->SetClocksForTesting(
        mock_task_runner_->GetMockClock(),
        mock_task_runner_->GetMockTickClock());

    return *factory_ptr;
  }

  [[nodiscard]] bool FetchRemoteCommands() {
    // A return value of |true| means the fetch command was successfully issued.
    return remote_commands_service_->FetchRemoteCommands();
  }

  // Return a builder for a signed RemoteCommand, with the important fields set
  // to valid defaults.
  SignedDataBuilder Command() {
    return std::move(
        SignedDataBuilder{}
            .SetCommandId(server_.GetNextCommandId())
            .SetCommandType(em::RemoteCommand_Type_COMMAND_ECHO_TEST)
            .SetTargetDeviceId(kDeviceId));
  }

  void FlushAllTasks() { mock_task_runner_->FastForwardUntilNoTasksRemain(); }

  PolicyInvalidationScope GetScope() const { return GetParam(); }

  const scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::TestMockTimeTaskRunner::Type::kBoundToThread);

  TestingRemoteCommandsServer server_;
  TestingCloudPolicyClientForRemoteCommands cloud_policy_client_{&server_,
                                                                 GetScope()};
  MockCloudPolicyStore store_;
  std::unique_ptr<RemoteCommandsService> remote_commands_service_;
};

TEST_P(RemoteCommandsServiceTest,
       ShouldCreateNoJobsIfServerHasNoRemoteCommands) {
  auto& job_factory = StartServiceWith<MockJobFactory>();

  EXPECT_NO_CALLS(job_factory, BuildJobForType);

  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();
}

TEST_P(RemoteCommandsServiceTest, ShouldCreateJobWhenRemoteCommandIsFetched) {
  auto& job_factory = StartServiceWith<FakeJobFactory>();

  server_.IssueCommand(
      Command()
          .SetCommandType(em::RemoteCommand_Type_DEVICE_FETCH_STATUS)
          .SetCommandPayload("the payload")
          .Build(),
      {});

  EXPECT_TRUE(FetchRemoteCommands());

  FakeJob& job = job_factory.WaitForJob();
  EXPECT_EQ(job.GetType(), em::RemoteCommand_Type_DEVICE_FETCH_STATUS);
  EXPECT_EQ(job.GetPayload(), "the payload");
  job.Finish();
}

TEST_P(RemoteCommandsServiceTest, ShouldSendJobSuccessToRemoteServer) {
  auto& job_factory = StartServiceWith<FakeJobFactory>();
  ServerResponseFuture response_future;
  server_.IssueCommand(Command().Build(), response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  job_factory.WaitForJob().FinishWithSuccess("<the-payload>");

  // Now we wait until the result of the job is received by the server.
  em::RemoteCommandResult result = response_future.Get();
  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
  EXPECT_EQ(result.payload(), "<the-payload>");
}

TEST_P(RemoteCommandsServiceTest, ShouldSendJobFailureToRemoteServer) {
  auto& job_factory = StartServiceWith<FakeJobFactory>();

  ServerResponseFuture response_future;
  server_.IssueCommand(Command().Build(), response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  job_factory.WaitForJob().FinishWithFailure("<the-failure-payload>");

  // Now we wait until the result of the job is received by the server.
  em::RemoteCommandResult result = response_future.Get();
  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_FAILURE);
  EXPECT_EQ(result.payload(), "<the-failure-payload>");
}

TEST_P(RemoteCommandsServiceTest, ShouldSendFailureToCreateJobToRemoteServer) {
  auto& job_factory = StartServiceWith<MockJobFactory>();

  ServerResponseFuture response_future;
  server_.IssueCommand(Command().Build(), response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  // Fail building of the job
  EXPECT_CALL(job_factory, BuildJobForType).WillOnce(Return(nullptr));

  // Now we wait until the result of the job is received by the server.
  em::RemoteCommandResult result = response_future.Get();
  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_IGNORED);
  EXPECT_EQ(result.payload(), "");
}

TEST_P(RemoteCommandsServiceTest,
       ShouldSupportMultipleRemoteCommandsSentTogether) {
  auto& job_factory = StartServiceWith<FakeJobFactory>();

  // Send 2 remote commands
  ServerResponseFuture first_future;
  server_.IssueCommand(Command().SetCommandPayload("first").Build(),
                       first_future.GetCallback());
  ServerResponseFuture second_future;
  server_.IssueCommand(Command().SetCommandPayload("second").Build(),
                       second_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  // Handle both jobs - in order.
  FakeJob& first_job = job_factory.WaitForJob();
  EXPECT_EQ(first_job.GetPayload(), "first");
  first_job.FinishWithSuccess("first-result");

  FakeJob& second_job = job_factory.WaitForJob();
  EXPECT_EQ(second_job.GetPayload(), "second");
  second_job.FinishWithSuccess("second-result");

  // Expect results to both remote commands on the server.
  em::RemoteCommandResult first_result = first_future.Get();
  EXPECT_EQ(first_result.payload(), "first-result");

  em::RemoteCommandResult second_result = second_future.Get();
  EXPECT_EQ(second_result.payload(), "second-result");
}

TEST_P(RemoteCommandsServiceTest,
       ShouldSupportMultipleRemoteCommandsSentBackToBack) {
  auto& job_factory = StartServiceWith<FakeJobFactory>();

  // Send the first remote command.
  ServerResponseFuture first_future;
  server_.IssueCommand(Command().SetCommandPayload("first").Build(),
                       first_future.GetCallback());

  EXPECT_TRUE(FetchRemoteCommands());

  // Send the second remote command after the first one is fetched.
  ServerResponseFuture second_future;
  server_.IssueCommand(Command().SetCommandPayload("second").Build(),
                       second_future.GetCallback());

  // The system should now allow us to handle both jobs
  // (by automatically fetching new remote commands when a job is finished).
  FakeJob& first_job = job_factory.WaitForJob();
  EXPECT_EQ(first_job.GetPayload(), "first");
  first_job.FinishWithSuccess("first-result");

  FakeJob& second_job = job_factory.WaitForJob();
  EXPECT_EQ(second_job.GetPayload(), "second");
  second_job.FinishWithSuccess("second-result");

  // And the result of both jobs should be sent to the server.
  em::RemoteCommandResult first_result = first_future.Get();
  EXPECT_EQ(first_result.payload(), "first-result");

  em::RemoteCommandResult second_result = second_future.Get();
  EXPECT_EQ(second_result.payload(), "second-result");
}

TEST_P(RemoteCommandsServiceTest, NewCommandFollowingFetch) {
  auto& job_factory = StartServiceWith<FakeJobFactory>();

  // Don't return anything on the first fetch.
  server_.OnNextFetchCommandsCallReturnNothing();

  // Add a command which will be issued after the first fetch.
  server_.IssueCommand(
      Command().SetCommandPayload("Command sent in the second fetch").Build(),
      {});

  // Attempt to fetch commands.
  EXPECT_TRUE(FetchRemoteCommands());

  // The command fetch should be in progress.
  EXPECT_TRUE(remote_commands_service_->IsCommandFetchInProgressForTesting());

  // And a following up fetch request should be enqueued.
  // A return value of |false| means exactly that - another fetch request is in
  // progress, but a follow up request has been enqueued.
  EXPECT_FALSE(FetchRemoteCommands());

  FakeJob& job = job_factory.WaitForJob();
  EXPECT_EQ(job.GetPayload(), "Command sent in the second fetch");
  job.Finish();
}

// Tests that the 'acked callback' gets called after the next response from the
// server.
TEST_P(RemoteCommandsServiceTest, AckedCallback) {
  auto& job_factory = StartServiceWith<FakeJobFactory>();

  // Fetch the command.
  ServerResponseFuture response_future;
  server_.IssueCommand(Command().Build(), response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  // Wait for the job to be created. This means the fetch is completed.
  FakeJob& job = job_factory.WaitForJob();

  // Now we install the ack callback, which should be invoked only after the
  // job finished and its result was uploaded to the server.
  StrictMock<base::MockOnceCallback<void()>> ack_callback;
  remote_commands_service_->SetOnCommandAckedCallback(ack_callback.Get());

  job.Finish();

  // Wait for the result to be uploaded to the server.
  ASSERT_TRUE(response_future.Wait());

  // Only now we should expect the ack callback to be called.
  EXPECT_CALL(ack_callback, Run());

  FlushAllTasks();
}

TEST_P(RemoteCommandsServiceTest, ShouldRejectCommandWithInvalidSignature) {
  auto& job_factory = StartServiceWith<MockJobFactory>();
  ServerResponseFuture response_future;

  server_.IssueCommand(Command().SetSignature("random-signature").Build(),
                       response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  EXPECT_NO_CALLS(job_factory, BuildJobForType);
  EXPECT_EQ(response_future.Get().result(),
            em::RemoteCommandResult_ResultType_RESULT_IGNORED);
}

TEST_P(RemoteCommandsServiceTest, ShouldRejectCommandWithInvalidSignedData) {
  auto& job_factory = StartServiceWith<MockJobFactory>();
  ServerResponseFuture response_future;

  server_.IssueCommand(Command().SetSignedData("random-data").Build(),
                       response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  EXPECT_NO_CALLS(job_factory, BuildJobForType);
  EXPECT_EQ(response_future.Get().result(),
            em::RemoteCommandResult_ResultType_RESULT_IGNORED);
}

TEST_P(RemoteCommandsServiceTest, ShouldRejectCommandWithInvalidPolicyType) {
  auto& job_factory = StartServiceWith<MockJobFactory>();
  ServerResponseFuture response_future;

  server_.IssueCommand(Command().SetPolicyType("random-policy-type").Build(),
                       response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  EXPECT_NO_CALLS(job_factory, BuildJobForType);
  EXPECT_EQ(response_future.Get().result(),
            em::RemoteCommandResult_ResultType_RESULT_IGNORED);
}

TEST_P(RemoteCommandsServiceTest, ShouldRejectCommandWithInvalidPolicyValue) {
  auto& job_factory = StartServiceWith<MockJobFactory>();
  ServerResponseFuture response_future;

  server_.IssueCommand(Command().SetPolicyValue("random-policy-value").Build(),
                       response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  EXPECT_NO_CALLS(job_factory, BuildJobForType);
  EXPECT_EQ(response_future.Get().result(),
            em::RemoteCommandResult_ResultType_RESULT_IGNORED);
}

TEST_P(RemoteCommandsServiceTest,
       ShouldRejectCommandWithInvalidTargetDeviceId) {
  auto& job_factory = StartServiceWith<MockJobFactory>();
  ServerResponseFuture response_future;

  server_.IssueCommand(Command().SetTargetDeviceId("wrong-device-id").Build(),
                       response_future.GetCallback());
  EXPECT_TRUE(FetchRemoteCommands());

  EXPECT_NO_CALLS(job_factory, BuildJobForType);
  EXPECT_EQ(response_future.Get().result(),
            em::RemoteCommandResult_ResultType_RESULT_IGNORED);
}

class RemoteCommandsServiceHistogramTest : public RemoteCommandsServiceTest {
 protected:
  using MetricReceivedRemoteCommand =
      RemoteCommandsService::MetricReceivedRemoteCommand;

  RemoteCommandsServiceHistogramTest() {
    StartServiceWith<NiceMock<MockJobFactory>>();
  }

  std::string GetMetricNameReceived() {
    return RemoteCommandsService::GetMetricNameReceivedRemoteCommand(
        GetScope());
  }

  std::string GetMetricNameExecuted() {
    return RemoteCommandsService::GetMetricNameExecutedRemoteCommand(
        GetScope(), em::RemoteCommand_Type_COMMAND_ECHO_TEST);
  }

  void ExpectReceivedCommandsMetrics(
      const std::vector<MetricReceivedRemoteCommand>& metrics) {
    for (const auto& metric : metrics) {
      histogram_tester_.ExpectBucketCount(GetMetricNameReceived(), metric, 1);
    }
    histogram_tester_.ExpectTotalCount(GetMetricNameReceived(), metrics.size());
  }

  void ExpectExecutedCommandsMetrics(
      const std::vector<RemoteCommandJob::Status>& metrics) {
    for (const auto& metric : metrics) {
      histogram_tester_.ExpectBucketCount(GetMetricNameExecuted(), metric, 1);
    }
    histogram_tester_.ExpectTotalCount(GetMetricNameExecuted(), metrics.size());
  }

 private:
  base::HistogramTester histogram_tester_;
};

TEST_P(RemoteCommandsServiceHistogramTest, WhenNoCommandsNothingRecorded) {
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenReceivedCommandOfUnknownTypeRecordUnknownType) {
  server_.IssueCommand(Command().ClearCommandType().Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({MetricReceivedRemoteCommand::kUnknownType});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenReceivedCommandWithoutIdRecordInvalid) {
  server_.IssueCommand(Command().ClearCommandId().Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({MetricReceivedRemoteCommand::kInvalid});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenReceivedExistingCommandRecordDuplicated) {
  server_.IssueCommand(Command().SetCommandId(222).Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  server_.IssueCommand(Command().SetCommandId(222).Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({MetricReceivedRemoteCommand::kCommandEchoTest,
                                 MetricReceivedRemoteCommand::kDuplicated});
  ExpectExecutedCommandsMetrics({RemoteCommandJob::SUCCEEDED});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenCannotBuildJobRecordInvalidScope) {
  auto& job_factory = StartServiceWith<MockJobFactory>();
  EXPECT_CALL(job_factory, BuildJobForType).WillOnce(Return(nullptr));

  server_.IssueCommand(Command().Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({MetricReceivedRemoteCommand::kInvalidScope});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenReceivedInvalidSignatureRecordInvalidSignature) {
  server_.IssueCommand(Command().SetSignature("wrong-signature").Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics(
      {MetricReceivedRemoteCommand::kInvalidSignature});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenReceivedInvalidPolicyDataRecordInvalid) {
  server_.IssueCommand(Command().SetPolicyType("random-policy-type").Build(),
                       {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({MetricReceivedRemoteCommand::kInvalid});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenReceivedInvalidTargetDeviceRecordInvalid) {
  server_.IssueCommand(Command().SetTargetDeviceId("invalid-device-id").Build(),
                       {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({MetricReceivedRemoteCommand::kInvalid});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest,
       WhenReceivedInvalidCommandRecordInvalid) {
  server_.IssueCommand(Command().SetPolicyValue("wrong-value").Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics({MetricReceivedRemoteCommand::kInvalid});
  ExpectExecutedCommandsMetrics({});
}

TEST_P(RemoteCommandsServiceHistogramTest, WhenReceivedValidCommandRecordType) {
  server_.IssueCommand(Command().Build(), {});
  EXPECT_TRUE(FetchRemoteCommands());
  FlushAllTasks();

  ExpectReceivedCommandsMetrics(
      {MetricReceivedRemoteCommand::kCommandEchoTest});
  ExpectExecutedCommandsMetrics({RemoteCommandJob::SUCCEEDED});
}

INSTANTIATE_TEST_SUITE_P(RemoteCommandsServiceTestInstance,
                         RemoteCommandsServiceTest,
                         testing::Values(PolicyInvalidationScope::kUser,
                                         PolicyInvalidationScope::kDevice,
                                         PolicyInvalidationScope::kCBCM));

INSTANTIATE_TEST_SUITE_P(RemoteCommandsServiceHistogramTestInstance,
                         RemoteCommandsServiceHistogramTest,
                         testing::Values(PolicyInvalidationScope::kUser,
                                         PolicyInvalidationScope::kDevice,
                                         PolicyInvalidationScope::kCBCM));
}  // namespace policy
