// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_
#define COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {

class MockReportQueueProvider : public ReportQueueProvider {
 public:
  MockReportQueueProvider();
  MockReportQueueProvider(const MockReportQueueProvider&) = delete;
  MockReportQueueProvider& operator=(const MockReportQueueProvider&) = delete;
  ~MockReportQueueProvider() override;

  // This method will make sure - by mocking - that CreateQueue on the provider
  // always returns a new std::unique_ptr<MockReportQueue> to simulate the
  // original behaviour. Note times is also added to be expected so you should
  // know how often you expect this method to be called.
  void ExpectCreateNewQueueAndReturnNewMockQueue(size_t times);

  // This method will make sure - by mocking - that CreateNewSpeculativeQueue on
  // the provider always returns a new std::unique_ptr<MockReportQueue,
  // base::OnTaskRunnerDeleter> on a sequenced task runner to simulate the
  // original behaviour. Note times is also added to be expected so you should
  // know how often you expect this method to be called.
  void ExpectCreateNewSpeculativeQueueAndReturnNewMockQueue(size_t times);

  // The following mocks will be invoked on the same thread
  // MockReportQueueProvider was constructed on.
  MOCK_METHOD(void,
              CreateNewQueueMock,
              (std::unique_ptr<ReportQueueConfiguration> config,
               CreateReportQueueCallback cb),
              ());

  MOCK_METHOD(
      (StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>),
      CreateNewSpeculativeQueueMock,
      (const ReportQueue::SpeculativeConfigSettings& config_settings),
      ());

  MOCK_METHOD(void, OnInitCompletedMock, (), ());

  MOCK_METHOD(void,
              ConfigureReportQueueMock,
              (std::unique_ptr<ReportQueueConfiguration> configuration,
               ReportQueueProvider::ReportQueueConfiguredCallback callback),
              ());

  void CheckOnThread() const;

 private:
  // Implementations of ReportQueueProvider virtual methods.
  void OnInitCompleted() override;
  void CreateNewQueue(std::unique_ptr<ReportQueueConfiguration> config,
                      CreateReportQueueCallback cb) override;
  StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
  CreateNewSpeculativeQueue(
      const ReportQueue::SpeculativeConfigSettings& config_settings) override;
  void ConfigureReportQueue(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config,
      ReportQueueConfiguredCallback completion_cb) override;

  scoped_refptr<StorageModuleInterface> storage_;
  SEQUENCE_CHECKER(test_sequence_checker_);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_
