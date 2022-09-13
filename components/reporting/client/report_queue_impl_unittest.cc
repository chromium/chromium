// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_impl.h"

#include <stdio.h>

#include <utility>

#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/test.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

using ::reporting::test::TestStorageModule;

namespace reporting {
namespace {

constexpr char kTestMessage[] = "TEST_MESSAGE";

// Creates a |ReportQueue| using |TestStorageModule| and
// |TestEncryptionModule|. Allows access to the storage module for checking
// stored values.
class ReportQueueImplTest : public testing::Test {
 protected:
  ReportQueueImplTest()
      : priority_(Priority::IMMEDIATE),
        dm_token_("FAKE_DM_TOKEN"),
        destination_(Destination::UPLOAD_EVENTS),
        storage_module_(base::MakeRefCounted<TestStorageModule>()),
        policy_check_callback_(
            base::BindRepeating(&MockFunction<Status()>::Call,
                                base::Unretained(&mocked_policy_check_))) {}

  void SetUp() override {
    ON_CALL(mocked_policy_check_, Call())
        .WillByDefault(Return(Status::StatusOK()));

    StatusOr<std::unique_ptr<ReportQueueConfiguration>> config_result =
        ReportQueueConfiguration::Create(dm_token_, destination_,
                                         policy_check_callback_);

    ASSERT_TRUE(config_result.ok());

    test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> report_queue_event;
    ReportQueueImpl::Create(std::move(config_result.ValueOrDie()),
                            storage_module_, report_queue_event.cb());
    auto report_queue_result = report_queue_event.result();
    ASSERT_TRUE(report_queue_result.ok());

    report_queue_ = std::move(report_queue_result.ValueOrDie());
  }

  TestStorageModule* test_storage_module() const {
    TestStorageModule* test_storage_module =
        google::protobuf::down_cast<TestStorageModule*>(storage_module_.get());
    DCHECK(test_storage_module);
    return test_storage_module;
  }

  NiceMock<MockFunction<Status()>> mocked_policy_check_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const Priority priority_;

  std::unique_ptr<ReportQueue> report_queue_;
  base::OnceCallback<void(Status)> callback_;

 private:
  const std::string dm_token_;
  const Destination destination_;
  scoped_refptr<StorageModuleInterface> storage_module_;
  ReportQueueConfiguration::PolicyCheckCallback policy_check_callback_;
};

// Enqueues a random string and ensures that the string arrives unaltered in the
// |StorageModuleInterface|.
TEST_F(ReportQueueImplTest, SuccessfulStringRecord) {
  constexpr char kTestString[] = "El-Chupacabra";
  test::TestEvent<Status> a;
  report_queue_->Enqueue(kTestString, priority_, a.cb());
  EXPECT_OK(a.result());
  EXPECT_EQ(test_storage_module()->priority(), priority_);
  EXPECT_EQ(test_storage_module()->record().data(), kTestString);
}

// Enqueues a |base::Value| dictionary and ensures it arrives unaltered in the
// |StorageModuleInterface|.
TEST_F(ReportQueueImplTest, SuccessfulBaseValueRecord) {
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value::Dict test_dict;
  test_dict.Set(kTestKey, kTestValue);
  test::TestEvent<Status> a;
  report_queue_->Enqueue(test_dict.Clone(), priority_, a.cb());
  EXPECT_OK(a.result());

  EXPECT_EQ(test_storage_module()->priority(), priority_);

  absl::optional<base::Value> value_result =
      base::JSONReader::Read(test_storage_module()->record().data());
  ASSERT_TRUE(value_result);
  EXPECT_EQ(value_result.value().GetDict(), test_dict);
}

// Enqueues a |TestMessage| and ensures that it arrives unaltered in the
// |StorageModuleInterface|.
TEST_F(ReportQueueImplTest, SuccessfulProtoRecord) {
  test::TestMessage test_message;
  test_message.set_test(kTestMessage);
  test::TestEvent<Status> a;
  report_queue_->Enqueue(std::make_unique<test::TestMessage>(test_message),
                         priority_, a.cb());
  EXPECT_OK(a.result());

  EXPECT_EQ(test_storage_module()->priority(), priority_);

  test::TestMessage result_message;
  ASSERT_TRUE(
      result_message.ParseFromString(test_storage_module()->record().data()));
  ASSERT_EQ(result_message.test(), test_message.test());
}

// The call to enqueue should succeed, indicating that the storage operation has
// been scheduled. The callback should fail, indicating that storage was
// unsuccessful.
TEST_F(ReportQueueImplTest, CallSuccessCallbackFailure) {
  EXPECT_CALL(*test_storage_module(), AddRecord(Eq(priority_), _, _))
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(Status)> callback) {
            std::move(callback).Run(Status(error::UNKNOWN, "Failing for Test"));
          })));

  test::TestMessage test_message;
  test_message.set_test(kTestMessage);
  test::TestEvent<Status> a;
  report_queue_->Enqueue(std::make_unique<test::TestMessage>(test_message),
                         priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

TEST_F(ReportQueueImplTest, EnqueueStringFailsOnPolicy) {
  EXPECT_CALL(mocked_policy_check_, Call())
      .WillOnce(Return(Status(error::UNAUTHENTICATED, "Failing for tests")));
  constexpr char kTestString[] = "El-Chupacabra";
  test::TestEvent<Status> a;
  report_queue_->Enqueue(std::string(kTestString), priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNAUTHENTICATED);
}

TEST_F(ReportQueueImplTest, EnqueueProtoFailsOnPolicy) {
  EXPECT_CALL(mocked_policy_check_, Call())
      .WillOnce(Return(Status(error::UNAUTHENTICATED, "Failing for tests")));
  test::TestMessage test_message;
  test_message.set_test(kTestMessage);
  test::TestEvent<Status> a;
  report_queue_->Enqueue(std::make_unique<test::TestMessage>(test_message),
                         priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNAUTHENTICATED);
}

TEST_F(ReportQueueImplTest, EnqueueValueFailsOnPolicy) {
  EXPECT_CALL(mocked_policy_check_, Call())
      .WillOnce(Return(Status(error::UNAUTHENTICATED, "Failing for tests")));
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value::Dict test_dict;
  test_dict.Set(kTestKey, kTestValue);
  test::TestEvent<Status> a;
  report_queue_->Enqueue(test_dict.Clone(), priority_, a.cb());
  const auto result = a.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNAUTHENTICATED);
}

TEST_F(ReportQueueImplTest, EnqueueAndFlushSuccess) {
  test::TestMessage test_message;
  test_message.set_test(kTestMessage);
  test::TestEvent<Status> a;
  report_queue_->Enqueue(std::make_unique<test::TestMessage>(test_message),
                         priority_, a.cb());
  EXPECT_OK(a.result());
  test::TestEvent<Status> f;
  report_queue_->Flush(priority_, f.cb());
  EXPECT_OK(f.result());
}

TEST_F(ReportQueueImplTest, EnqueueSuccessFlushFailure) {
  test::TestMessage test_message;
  test_message.set_test(kTestMessage);
  test::TestEvent<Status> a;
  report_queue_->Enqueue(std::make_unique<test::TestMessage>(test_message),
                         priority_, a.cb());
  EXPECT_OK(a.result());

  EXPECT_CALL(*test_storage_module(), Flush(Eq(priority_), _))
      .WillOnce(
          WithArg<1>(Invoke([](base::OnceCallback<void(Status)> callback) {
            std::move(callback).Run(Status(error::UNKNOWN, "Failing for Test"));
          })));
  test::TestEvent<Status> f;
  report_queue_->Flush(priority_, f.cb());
  const auto result = f.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

// Enqueues a random string into speculative queue, then enqueues a sting,
// attaches actual one and ensures that the string arrives unaltered in the
// |StorageModuleInterface|.
TEST_F(ReportQueueImplTest, SuccessfulSpeculativeStringRecord) {
  constexpr char kTestString[] = "El-Chupacabra";
  test::TestEvent<Status> a;
  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();
  speculative_report_queue->Enqueue(std::string(kTestString), priority_,
                                    a.cb());
  EXPECT_OK(a.result());

  speculative_report_queue->AttachActualQueue(std::move(report_queue_));
  // Let everything ongoing to finish.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_storage_module()->priority(), priority_);
  EXPECT_EQ(test_storage_module()->record().data(), kTestString);
}

TEST_F(ReportQueueImplTest, SpeculativeQueueMultipleRecordsAfterCreation) {
  constexpr char kTestString1[] = "record1";
  constexpr char kTestString2[] = "record2";
  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();

  speculative_report_queue->AttachActualQueue(std::move(report_queue_));
  // Let everything ongoing to finish.
  task_environment_.RunUntilIdle();

  test::TestEvent<Status> test_event1;
  speculative_report_queue->Enqueue(kTestString1, Priority::IMMEDIATE,
                                    test_event1.cb());
  const auto result1 = test_event1.result();
  ASSERT_TRUE(result1.ok());
  EXPECT_EQ(test_storage_module()->priority(), Priority::IMMEDIATE);
  EXPECT_EQ(test_storage_module()->record().data(), kTestString1);

  test::TestEvent<Status> test_event2;
  speculative_report_queue->Enqueue(kTestString2, Priority::SLOW_BATCH,
                                    test_event2.cb());
  const auto result2 = test_event2.result();
  ASSERT_TRUE(result2.ok());
  EXPECT_EQ(test_storage_module()->priority(), Priority::SLOW_BATCH);
  EXPECT_EQ(test_storage_module()->record().data(), kTestString2);
}

TEST_F(ReportQueueImplTest, SpeculativeQueueCreationFailed) {
  constexpr char kTestString[] = "record";
  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();

  auto attach_cb = speculative_report_queue->PrepareToAttachActualQueue();
  std::move(attach_cb).Run(Status(error::UNKNOWN, "error msg"));
  task_environment_.RunUntilIdle();

  test::TestEvent<Status> test_event;
  speculative_report_queue->Enqueue(kTestString, Priority::IMMEDIATE,
                                    test_event.cb());
  const auto result = test_event.result();
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.code(), error::UNKNOWN);
}

TEST_F(ReportQueueImplTest, OverlappingStringRecords) {
  constexpr char kTestString1[] = "record1";
  constexpr char kTestString2[] = "record2";
  constexpr char kTestString3[] = "record3";

  test::TestEvent<Status> event1;
  test::TestEvent<Status> event2;
  test::TestEvent<Status> event3;
  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();

  // Call `Enqueue` for 2 records before report queue is ready, both will be
  // added to pending records.
  speculative_report_queue->Enqueue(std::string(kTestString1), priority_,
                                    event1.cb());
  EXPECT_OK(event1.result());
  speculative_report_queue->Enqueue(std::string(kTestString2), priority_,
                                    event2.cb());
  EXPECT_OK(event2.result());

  base::queue<ReportQueue::EnqueueCallback> enqueue_cb_queue;
  int enqueue_count = 0;
  auto mock_queue = std::make_unique<MockReportQueue>();
  EXPECT_CALL(*mock_queue, AddRecord)
      .Times(3)
      .WillRepeatedly([&enqueue_cb_queue, &enqueue_count](
                          std::string record_string, Priority event_priority,
                          ReportQueue::EnqueueCallback cb) {
        ++enqueue_count;
        enqueue_cb_queue.emplace(std::move(cb));
      });

  // First record should be enqueued after calling `AttachActualQueue`.
  speculative_report_queue->AttachActualQueue(std::move(mock_queue));
  // Second record should be enqueued after calling `Enqueue` for the third
  // record, and third record should be added to pending records.
  speculative_report_queue->Enqueue(std::string(kTestString3), priority_,
                                    event3.cb());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(enqueue_count, 2);

  // Executing the first callback with success status should cause the third
  // record to be enqueued and pending records to be empty.
  ASSERT_THAT(enqueue_cb_queue, testing::SizeIs(2));
  std::move(enqueue_cb_queue.front()).Run(Status());
  enqueue_cb_queue.pop();
  task_environment_.RunUntilIdle();
  ASSERT_EQ(enqueue_count, 3);

  // Executing the second and third callback.
  ASSERT_THAT(enqueue_cb_queue, testing::SizeIs(2));
  std::move(enqueue_cb_queue.front()).Run(Status());
  enqueue_cb_queue.pop();
  task_environment_.RunUntilIdle();

  ASSERT_THAT(enqueue_cb_queue, testing::SizeIs(1));
  std::move(enqueue_cb_queue.front()).Run(Status());
  enqueue_cb_queue.pop();
  EXPECT_OK(event3.result());
  task_environment_.RunUntilIdle();
}

TEST_F(ReportQueueImplTest, EnqueueRecordWithInvalidPriority) {
  test::TestEvent<Status> event;
  report_queue_->Enqueue(std::string(kTestMessage),
                         Priority::UNDEFINED_PRIORITY, event.cb());
  const auto result = event.result();

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.code(), error::INVALID_ARGUMENT);
}

TEST_F(ReportQueueImplTest, FlushSpeculativeReportQueue) {
  test::TestEvent<Status> event;

  // Set up speculative report queue
  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();
  speculative_report_queue->AttachActualQueue(std::move(report_queue_));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*test_storage_module(), Flush(Eq(priority_), _))
      .WillOnce(
          WithArg<1>(Invoke([](base::OnceCallback<void(Status)> callback) {
            std::move(callback).Run(Status::StatusOK());
          })));

  speculative_report_queue->Flush(priority_, event.cb());
  const auto result = event.result();
  ASSERT_OK(result);
}

TEST_F(ReportQueueImplTest, FlushUninitializedSpeculativeReportQueue) {
  test::TestEvent<Status> event;

  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();
  speculative_report_queue->Flush(priority_, event.cb());

  const auto result = event.result();
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::FAILED_PRECONDITION);
}

TEST_F(ReportQueueImplTest, FlushFailedSpeculativeReportQueue) {
  test::TestEvent<Status> event;

  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();
  auto attach_cb = speculative_report_queue->PrepareToAttachActualQueue();
  std::move(attach_cb).Run(Status(error::UNKNOWN, "error msg"));
  task_environment_.RunUntilIdle();

  speculative_report_queue->Flush(priority_, event.cb());

  const auto result = event.result();
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

TEST_F(ReportQueueImplTest, AsyncProcessingReportQueue) {
  auto mock_queue = std::make_unique<MockReportQueue>();
  EXPECT_CALL(*mock_queue, AddProducedRecord)
      .Times(3)
      .WillRepeatedly([](ReportQueue::RecordProducer record_producer,
                         Priority event_priority,
                         ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status::StatusOK());
      });

  test::TestEvent<Status> a_string;
  mock_queue->Enqueue(std::string(kTestMessage), priority_, a_string.cb());

  test::TestEvent<Status> a_proto;
  test::TestMessage test_message;
  test_message.set_test(kTestMessage);
  mock_queue->Enqueue(std::make_unique<test::TestMessage>(test_message),
                      priority_, a_proto.cb());

  test::TestEvent<Status> a_json;
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value::Dict test_dict;
  test_dict.Set(kTestKey, kTestValue);
  mock_queue->Enqueue(std::move(test_dict), priority_, a_json.cb());

  EXPECT_OK(a_string.result());
  EXPECT_OK(a_proto.result());
  EXPECT_OK(a_json.result());
}

TEST_F(ReportQueueImplTest, AsyncProcessingSpeculativeReportQueue) {
  auto speculative_report_queue = SpeculativeReportQueueImpl::Create();

  test::TestEvent<Status> a_string;
  speculative_report_queue->Enqueue(std::string(kTestMessage), priority_,
                                    a_string.cb());

  test::TestEvent<Status> a_proto;
  test::TestMessage test_message;
  test_message.set_test(kTestMessage);
  speculative_report_queue->Enqueue(
      std::make_unique<test::TestMessage>(test_message), priority_,
      a_proto.cb());

  test::TestEvent<Status> a_json;
  constexpr char kTestKey[] = "TEST_KEY";
  constexpr char kTestValue[] = "TEST_VALUE";
  base::Value::Dict test_dict;
  test_dict.Set(kTestKey, kTestValue);
  speculative_report_queue->Enqueue(std::move(test_dict), priority_,
                                    a_json.cb());

  EXPECT_OK(a_string.result());
  EXPECT_OK(a_proto.result());
  EXPECT_OK(a_json.result());

  auto mock_queue = std::make_unique<MockReportQueue>();
  EXPECT_CALL(*mock_queue, AddProducedRecord)
      .Times(3)
      .WillRepeatedly([](ReportQueue::RecordProducer record_producer,
                         Priority event_priority,
                         ReportQueue::EnqueueCallback cb) {
        std::move(cb).Run(Status::StatusOK());
      });
  speculative_report_queue->AttachActualQueue(std::move(mock_queue));
  // Let everything ongoing to finish.
  task_environment_.RunUntilIdle();
}
}  // namespace
}  // namespace reporting
