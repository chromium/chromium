// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::WithoutArgs;

namespace reporting {
namespace {

class MockHealthModuleDelegate : public HealthModuleDelegate {
 public:
  MOCK_METHOD(Status, DoInit, (), (override));
  MOCK_METHOD(void, DoGetERPHealthData, (HealthCallback cb), (const override));
  MOCK_METHOD(void,
              DoPostHealthRecord,
              (HealthDataHistory history),
              (override));
};

HealthDataHistory AddEnqueueRecordCall() {
  HealthDataHistory history;
  EnqueueRecordCall call;
  call.set_priority(Priority::IMMEDIATE);
  *history.mutable_enqueue_record_call() = call;
  history.set_timestamp_seconds(base::Time::Now().ToTimeT());
  return history;
}

class HealthModuleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    delegate_ = std::make_unique<MockHealthModuleDelegate>();
    mock_delegate_ = delegate_.get();
  }

  void TearDown() override {
    mock_delegate_ = nullptr;  // Prevent runaway pointer.
  }

  void CreateModule() {
    // Next line will asynchronously invoke delegate_->Init.
    module_ = HealthModule::Create(std::move(delegate_));
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<MockHealthModuleDelegate> delegate_;
  raw_ptr<MockHealthModuleDelegate> mock_delegate_ = nullptr;
  scoped_refptr<HealthModule> module_;
};

TEST_F(HealthModuleTest, Init) {
  test::TestCallbackAutoWaiter init_waiter;
  EXPECT_CALL(*mock_delegate_, DoInit())
      .WillOnce(
          DoAll(Invoke(&init_waiter, &test::TestCallbackAutoWaiter::Signal),
                Return(Status::StatusOK())));
  CreateModule();
}

TEST_F(HealthModuleTest, InitFails) {
  test::TestCallbackAutoWaiter init_waiter;
  EXPECT_CALL(*mock_delegate_, DoInit())
      .WillOnce(
          DoAll(Invoke(&init_waiter, &test::TestCallbackAutoWaiter::Signal),
                Return(Status(error::UNKNOWN, "Test fails init"))));
  CreateModule();
}

TEST_F(HealthModuleTest, WriteAndReadData) {
  {
    test::TestCallbackAutoWaiter init_waiter;
    EXPECT_CALL(*mock_delegate_, DoInit())
        .WillOnce(
            DoAll(Invoke(&init_waiter, &test::TestCallbackAutoWaiter::Signal),
                  Return(Status::StatusOK())));
    CreateModule();
  }

  ERPHealthData ref_data;
  auto call = AddEnqueueRecordCall();
  *ref_data.add_history() = call;
  {
    test::TestCallbackAutoWaiter post_waiter;
    EXPECT_CALL(*mock_delegate_, DoPostHealthRecord(EqualsProto(call)))
        .WillOnce(WithoutArgs(
            Invoke(&post_waiter, &test::TestCallbackAutoWaiter::Signal)));
    module_->PostHealthRecord(call);
  }

  test::TestEvent<const ERPHealthData> read_event;
  EXPECT_CALL(*mock_delegate_, DoGetERPHealthData(_))
      .WillOnce(Invoke(
          [&ref_data](HealthCallback cb) { std::move(cb).Run(ref_data); }));
  module_->GetHealthData(read_event.cb());
  EXPECT_THAT(read_event.ref_result(), EqualsProto(ref_data));
}

TEST_F(HealthModuleTest, UseRecorder) {
  {
    test::TestCallbackAutoWaiter init_waiter;
    EXPECT_CALL(*mock_delegate_, DoInit())
        .WillOnce(
            DoAll(Invoke(&init_waiter, &test::TestCallbackAutoWaiter::Signal),
                  Return(Status::StatusOK())));
    CreateModule();
  }

  test::TestCallbackAutoWaiter post_waiter;
  auto call = AddEnqueueRecordCall();
  EXPECT_CALL(*mock_delegate_, DoPostHealthRecord(EqualsProto(call)))
      .WillOnce(WithoutArgs(
          Invoke(&post_waiter, &test::TestCallbackAutoWaiter::Signal)));
  auto recorder = HealthModule::Recorder(module_);
  // Hand recorder over for async processing.
  // PostHealthRecord will be called by its destructor.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce([](HealthModule::Recorder recorder,
                        HealthDataHistory call) { *recorder = call; },
                     std::move(recorder), call));
}
}  // namespace
}  // namespace reporting
