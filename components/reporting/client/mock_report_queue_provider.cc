// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/mock_report_queue_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/storage/test_storage_module.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace reporting {

MockReportQueueProvider::MockReportQueueProvider()
    : ReportQueueProvider(
          base::BindRepeating(
              [](OnStorageModuleCreatedCallback storage_created_cb) {
                std::move(storage_created_cb)
                    .Run(base::MakeRefCounted<test::TestStorageModule>());
              }),
          base::SequencedTaskRunner::GetCurrentDefault()) {}

MockReportQueueProvider::~MockReportQueueProvider() = default;

void MockReportQueueProvider::ExpectCreateNewQueueAndReturnNewMockQueue(
    size_t times) {
  CheckOnThread();

  EXPECT_CALL(*this, CreateNewQueueMock(_, _))
      .Times(times)
      .WillRepeatedly([](std::unique_ptr<ReportQueueConfiguration> config,
                         CreateReportQueueCallback cb) {
        std::move(cb).Run(std::make_unique<MockReportQueue>());
      });
}

void MockReportQueueProvider::
    ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(size_t times) {
  CheckOnThread();

  EXPECT_CALL(*this, CreateNewSpeculativeQueueMock(_))
      .Times(times)
      .WillRepeatedly([]() {
        auto report_queue =
            std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
                new MockReportQueue(),
                base::OnTaskRunnerDeleter(
                    base::ThreadPool::CreateSequencedTaskRunner({})));

        // Mock PrepareToAttachActualQueue so we do not attempt to replace
        // the mocked report queue
        EXPECT_CALL(*report_queue, PrepareToAttachActualQueue()).WillOnce([]() {
          return base::DoNothing();
        });

        return report_queue;
      });
}

void MockReportQueueProvider::OnInitCompleted() {
  CheckOnThread();
  OnInitCompletedMock();
}

void MockReportQueueProvider::CreateNewQueue(
    std::unique_ptr<ReportQueueConfiguration> config,
    CreateReportQueueCallback cb) {
  CheckOnThread();
  CreateNewQueueMock(std::move(config), std::move(cb));
}

StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
MockReportQueueProvider::CreateNewSpeculativeQueue(
    const ReportQueue::SpeculativeConfigSettings& config_settings) {
  CheckOnThread();
  return CreateNewSpeculativeQueueMock(config_settings);
}

void MockReportQueueProvider::ConfigureReportQueue(
    std::unique_ptr<ReportQueueConfiguration> report_queue_config,
    ReportQueueConfiguredCallback completion_cb) {
  CheckOnThread();
  ConfigureReportQueueMock(std::move(report_queue_config),
                           std::move(completion_cb));
}

void MockReportQueueProvider::CheckOnThread() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);
}
}  // namespace reporting
