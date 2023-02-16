// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_queue.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/test_support/echo_remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace em = enterprise_management;

namespace {

const RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
const RemoteCommandJob::UniqueIDType kUniqueID2 = 987654321;
const char kPayload[] = "_PAYLOAD_FOR_TESTING_";
const char kPayload2[] = "_PAYLOAD_FOR_TESTING2_";

em::RemoteCommand GenerateCommandProto(RemoteCommandJob::UniqueIDType unique_id,
                                       base::TimeDelta age_of_command,
                                       const std::string& payload) {
  em::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_COMMAND_ECHO_TEST);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  if (!payload.empty()) {
    command_proto.set_payload(payload);
  }
  return command_proto;
}

// Mock class for RemoteCommandsQueue::Observer.
class MockRemoteCommandsQueueObserver : public RemoteCommandsQueue::Observer {
 public:
  MockRemoteCommandsQueueObserver() = default;
  MockRemoteCommandsQueueObserver(const MockRemoteCommandsQueueObserver&) =
      delete;
  MockRemoteCommandsQueueObserver& operator=(
      const MockRemoteCommandsQueueObserver&) = delete;

  // RemoteCommandsQueue::Observer:
  MOCK_METHOD1(OnJobStarted, void(RemoteCommandJob* command));
  MOCK_METHOD1(OnJobFinished, void(RemoteCommandJob* command));
};

}  // namespace

using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::StrictMock;

class RemoteCommandsQueueTest : public testing::Test {
 public:
  RemoteCommandsQueueTest(const RemoteCommandsQueueTest&) = delete;
  RemoteCommandsQueueTest& operator=(const RemoteCommandsQueueTest&) = delete;

 protected:
  RemoteCommandsQueueTest();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void InitializeJob(RemoteCommandJob* job,
                     RemoteCommandJob::UniqueIDType unique_id,
                     base::TimeTicks issued_time,
                     const std::string& payload);
  void FailInitializeJob(RemoteCommandJob* job,
                         RemoteCommandJob::UniqueIDType unique_id,
                         base::TimeTicks issued_time,
                         const std::string& payload);

  void AddJobAndVerifyRunningAfter(std::unique_ptr<RemoteCommandJob> job,
                                   base::TimeDelta delta);

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  RemoteCommandsQueue queue_;
  StrictMock<MockRemoteCommandsQueueObserver> observer_;
  base::TimeTicks test_start_time_;
  raw_ptr<const base::Clock> clock_;
  raw_ptr<const base::TickClock> tick_clock_;

 private:
  void VerifyCommandIssuedTime(RemoteCommandJob* job,
                               base::TimeTicks expected_issued_time);

  base::SingleThreadTaskRunner::CurrentDefaultHandle
      runner_current_default_handle_;
};

RemoteCommandsQueueTest::RemoteCommandsQueueTest()
    : task_runner_(new base::TestMockTimeTaskRunner()),
      clock_(nullptr),
      tick_clock_(nullptr),
      runner_current_default_handle_(task_runner_) {}

void RemoteCommandsQueueTest::SetUp() {
  clock_ = task_runner_->GetMockClock();
  tick_clock_ = task_runner_->GetMockTickClock();
  test_start_time_ = tick_clock_->NowTicks();
  queue_.SetClocksForTesting(clock_, tick_clock_);
  queue_.AddObserver(&observer_);
}

void RemoteCommandsQueueTest::TearDown() {
  queue_.RemoveObserver(&observer_);
}

void RemoteCommandsQueueTest::InitializeJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    const std::string& payload) {
  EXPECT_TRUE(
      job->Init(tick_clock_->NowTicks(),
                GenerateCommandProto(
                    unique_id, tick_clock_->NowTicks() - issued_time, payload),
                em::SignedData()));
  EXPECT_EQ(unique_id, job->unique_id());
  VerifyCommandIssuedTime(job, issued_time);
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

void RemoteCommandsQueueTest::FailInitializeJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    const std::string& payload) {
  EXPECT_FALSE(
      job->Init(tick_clock_->NowTicks(),
                GenerateCommandProto(
                    unique_id, tick_clock_->NowTicks() - issued_time, payload),
                em::SignedData()));
  EXPECT_EQ(RemoteCommandJob::INVALID, job->status());
}

void RemoteCommandsQueueTest::AddJobAndVerifyRunningAfter(
    std::unique_ptr<RemoteCommandJob> job,
    base::TimeDelta delta) {
  Mock::VerifyAndClearExpectations(&observer_);

  const base::Time now = clock_->Now();

  // Add the job to the queue. It should start executing immediately.
  EXPECT_CALL(
      observer_,
      OnJobStarted(
          AllOf(Property(&RemoteCommandJob::status, RemoteCommandJob::RUNNING),
                Property(&RemoteCommandJob::execution_started_time, now))));
  queue_.AddJob(std::move(job));
  Mock::VerifyAndClearExpectations(&observer_);

  // After |delta|, the job should still be running.
  task_runner_->FastForwardBy(delta);
  Mock::VerifyAndClearExpectations(&observer_);
}

void RemoteCommandsQueueTest::VerifyCommandIssuedTime(
    RemoteCommandJob* job,
    base::TimeTicks expected_issued_time) {
  // Maximum possible error can be 1 millisecond due to truncating.
  EXPECT_GE(expected_issued_time, job->issued_time());
  EXPECT_LE(expected_issued_time - base::Milliseconds(1), job->issued_time());
}

TEST_F(RemoteCommandsQueueTest, SingleSucceedCommand) {
  // Initialize a job expected to succeed after 5 seconds, from a protobuf with
  // |kUniqueID|, |kPayload| and |test_start_time_| as command issued time.
  std::unique_ptr<RemoteCommandJob> job(
      new EchoRemoteCommandJob(true, base::Seconds(5)));
  InitializeJob(job.get(), kUniqueID, test_start_time_, kPayload);

  AddJobAndVerifyRunningAfter(std::move(job), base::Seconds(4));

  // After 6 seconds, the job is expected to be finished.
  EXPECT_CALL(observer_,
              OnJobFinished(AllOf(Property(&RemoteCommandJob::status,
                                           RemoteCommandJob::SUCCEEDED),
                                  Property(&RemoteCommandJob::GetResultPayload,
                                           Pointee(StrEq(kPayload))))));
  task_runner_->FastForwardBy(base::Seconds(2));
  Mock::VerifyAndClearExpectations(&observer_);

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(RemoteCommandsQueueTest, SingleFailedCommand) {
  // Initialize a job expected to fail after 10 seconds, from a protobuf with
  // |kUniqueID|, |kPayload| and |test_start_time_| as command issued time.
  std::unique_ptr<RemoteCommandJob> job(
      new EchoRemoteCommandJob(false, base::Seconds(10)));
  InitializeJob(job.get(), kUniqueID, test_start_time_, kPayload);

  AddJobAndVerifyRunningAfter(std::move(job), base::Seconds(9));

  // After 11 seconds, the job is expected to be finished.
  EXPECT_CALL(observer_,
              OnJobFinished(AllOf(
                  Property(&RemoteCommandJob::status, RemoteCommandJob::FAILED),
                  Property(&RemoteCommandJob::GetResultPayload,
                           Pointee(StrEq(kPayload))))));
  task_runner_->FastForwardBy(base::Seconds(2));
  Mock::VerifyAndClearExpectations(&observer_);

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(RemoteCommandsQueueTest, SingleTerminatedCommand) {
  // Initialize a job expected to fail after 600 seconds, from a protobuf with
  // |kUniqueID|, |kPayload| and |test_start_time_| as command issued time.
  std::unique_ptr<RemoteCommandJob> job(
      new EchoRemoteCommandJob(false, base::Seconds(600)));
  InitializeJob(job.get(), kUniqueID, test_start_time_, kPayload);

  AddJobAndVerifyRunningAfter(std::move(job), base::Seconds(599));

  // After 601 seconds, the job is expected to be terminated (10 minutes is the
  // timeout duration).
  EXPECT_CALL(observer_, OnJobFinished(Property(&RemoteCommandJob::status,
                                                RemoteCommandJob::TERMINATED)));
  task_runner_->FastForwardBy(base::Seconds(2));
  Mock::VerifyAndClearExpectations(&observer_);

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(RemoteCommandsQueueTest, SingleMalformedCommand) {
  // Initialize a job expected to succeed after 10 seconds, from a protobuf with
  // |kUniqueID|, |kMalformedCommandPayload| and |test_start_time_|.
  std::unique_ptr<RemoteCommandJob> job(
      new EchoRemoteCommandJob(true, base::Seconds(10)));
  // Should failed immediately.
  FailInitializeJob(job.get(), kUniqueID, test_start_time_,
                    EchoRemoteCommandJob::kMalformedCommandPayload);
}

TEST_F(RemoteCommandsQueueTest, SingleExpiredCommand) {
  // Initialize a job expected to succeed after 10 seconds, from a protobuf with
  // |kUniqueID| and |test_start_time_ - 4 hours|.
  std::unique_ptr<RemoteCommandJob> job(
      new EchoRemoteCommandJob(true, base::Seconds(10)));
  InitializeJob(job.get(), kUniqueID, test_start_time_ - base::Hours(4),
                std::string());

  // Add the job to the queue. It should not be started.
  EXPECT_CALL(observer_, OnJobFinished(Property(&RemoteCommandJob::status,
                                                RemoteCommandJob::EXPIRED)));
  queue_.AddJob(std::move(job));
  Mock::VerifyAndClearExpectations(&observer_);

  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(RemoteCommandsQueueTest, TwoCommands) {
  InSequence sequence;

  // Initialize a job expected to succeed after 5 seconds, from a protobuf with
  // |kUniqueID|, |kPayload| and |test_start_time_| as command issued time.
  std::unique_ptr<RemoteCommandJob> job(
      new EchoRemoteCommandJob(true, base::Seconds(5)));
  InitializeJob(job.get(), kUniqueID, test_start_time_, kPayload);

  // Add the job to the queue, should start executing immediately. Pass the
  // ownership of |job| as well.
  EXPECT_CALL(
      observer_,
      OnJobStarted(AllOf(
          Property(&RemoteCommandJob::unique_id, kUniqueID),
          Property(&RemoteCommandJob::status, RemoteCommandJob::RUNNING))));
  queue_.AddJob(std::move(job));
  Mock::VerifyAndClearExpectations(&observer_);

  // Initialize another job expected to succeed after 5 seconds, from a protobuf
  // with |kUniqueID2|, |kPayload2| and |test_start_time_ + 1s| as command
  // issued time.
  job = std::make_unique<EchoRemoteCommandJob>(true, base::Seconds(5));
  InitializeJob(job.get(), kUniqueID2, test_start_time_ + base::Seconds(1),
                kPayload2);

  // After 2 seconds, add the second job. It should be queued and not start
  // running immediately.
  task_runner_->FastForwardBy(base::Seconds(2));
  queue_.AddJob(std::move(job));

  // After 4 seconds, nothing happens.
  task_runner_->FastForwardBy(base::Seconds(2));
  Mock::VerifyAndClearExpectations(&observer_);

  // After 6 seconds, the first job should finish running and the second one
  // start immediately after that.
  EXPECT_CALL(
      observer_,
      OnJobFinished(AllOf(
          Property(&RemoteCommandJob::unique_id, kUniqueID),
          Property(&RemoteCommandJob::status, RemoteCommandJob::SUCCEEDED),
          Property(&RemoteCommandJob::GetResultPayload,
                   Pointee(StrEq(kPayload))))));
  EXPECT_CALL(
      observer_,
      OnJobStarted(AllOf(
          Property(&RemoteCommandJob::unique_id, kUniqueID2),
          Property(&RemoteCommandJob::status, RemoteCommandJob::RUNNING))));
  task_runner_->FastForwardBy(base::Seconds(2));
  Mock::VerifyAndClearExpectations(&observer_);

  // After 11 seconds, the second job should finish running as well.
  EXPECT_CALL(
      observer_,
      OnJobFinished(AllOf(
          Property(&RemoteCommandJob::unique_id, kUniqueID2),
          Property(&RemoteCommandJob::status, RemoteCommandJob::SUCCEEDED),
          Property(&RemoteCommandJob::GetResultPayload,
                   Pointee(StrEq(kPayload2))))));
  task_runner_->FastForwardBy(base::Seconds(5));
  Mock::VerifyAndClearExpectations(&observer_);

  task_runner_->FastForwardUntilNoTasksRemain();
}

}  // namespace policy
