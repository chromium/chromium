// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider_test_helper.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrEq;
using ::testing::WithArg;

namespace reporting {
namespace {

class ReportQueueProviderTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const Destination destination_ = Destination::UPLOAD_EVENTS;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

TEST_F(ReportQueueProviderTest, CreateAndGetQueue) {
  std::unique_ptr<MockReportQueueProvider> provider =
      std::make_unique<NiceMock<MockReportQueueProvider>>();
  report_queue_provider_test_helper::SetForTesting(provider.get());

  static constexpr char kTestMessage[] = "TEST MESSAGE";
  // Create configuration.
  auto config_result = ReportQueueConfiguration::Create(
      EventType::kDevice, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);
  EXPECT_CALL(*provider.get(), OnInitCompletedMock()).Times(1);
  provider->ExpectCreateNewQueueAndReturnNewMockQueue(1);
  // Use it to asynchronously create ReportingQueue and then asynchronously
  // send the message.
  test::TestEvent<Status> e;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::StringPiece data, ReportQueue::EnqueueCallback done_cb,
             std::unique_ptr<ReportQueueConfiguration> config) {
            // Asynchronously create ReportingQueue.
            base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
                queue_cb = base::BindOnce(
                    [](std::string data, ReportQueue::EnqueueCallback done_cb,
                       StatusOr<std::unique_ptr<ReportQueue>>
                           report_queue_result) {
                      // Bail out if queue failed to create.
                      if (!report_queue_result.ok()) {
                        std::move(done_cb).Run(report_queue_result.status());
                        return;
                      }
                      // Queue created successfully, enqueue the message.
                      EXPECT_CALL(*static_cast<MockReportQueue*>(
                                      report_queue_result.ValueOrDie().get()),
                                  AddRecord(StrEq(data), Eq(FAST_BATCH), _))
                          .WillOnce(WithArg<2>(
                              Invoke([](ReportQueue::EnqueueCallback cb) {
                                std::move(cb).Run(Status::StatusOK());
                              })));
                      report_queue_result.ValueOrDie()->Enqueue(
                          std::move(data), FAST_BATCH, std::move(done_cb));
                    },
                    std::string(data), std::move(done_cb));
            ReportQueueProvider::CreateQueue(std::move(config),
                                             std::move(queue_cb));
          },
          kTestMessage, e.cb(), std::move(config_result.ValueOrDie())));
  const auto res = e.result();
  EXPECT_OK(res) << res;
  report_queue_provider_test_helper::SetForTesting(nullptr);
}

TEST_F(ReportQueueProviderTest,
       CreateReportQueueWithEncryptedReportingPipelineDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ReportQueueProvider::kEncryptedReportingPipeline);

  // Create configuration
  auto config_result = ReportQueueConfiguration::Create(
      EventType::kDevice, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);

  test::TestEvent<ReportQueueProvider::CreateReportQueueResponse> event;
  ReportQueueProvider::CreateQueue(std::move(config_result.ValueOrDie()),
                                   event.cb());
  const auto result = event.result();

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), error::FAILED_PRECONDITION);
}

TEST_F(ReportQueueProviderTest,
       CreateSpeculativeReportQueueWithEncryptedReportingPipelineDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ReportQueueProvider::kEncryptedReportingPipeline);

  // Create configuration
  auto config_result = ReportQueueConfiguration::Create(
      EventType::kDevice, destination_, policy_checker_callback_);
  ASSERT_OK(config_result);

  const auto result = ReportQueueProvider::CreateSpeculativeQueue(
      std::move(config_result.ValueOrDie()));
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), error::FAILED_PRECONDITION);
}

}  // namespace
}  // namespace reporting
