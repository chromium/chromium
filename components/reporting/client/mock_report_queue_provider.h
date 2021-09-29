// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_
#define COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_

#include <memory>

#include "base/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {

class MockReportQueueProvider : public ReportQueueProvider {
 public:
  MockReportQueueProvider();
  MockReportQueueProvider(const MockReportQueueProvider&) = delete;
  MockReportQueueProvider& operator=(const MockReportQueueProvider&) = delete;
  ~MockReportQueueProvider() override;

  InitializingContext* InstantiateInitializingContext(
      InitCompleteCallback init_complete_cb,
      scoped_refptr<InitializationStateTracker> init_state_tracker) override;

  // This method will make sure - by mocking - that CreateQueue on the provider
  // always returns a new std::unique_ptr<MockReportQueue> to simulate the
  // original behaviour. Note times is also added to be expected so you should
  // know how often you expect this method to be called.
  void ExpectCreateNewQueueAndReturnNewMockQueue(int times);

  MOCK_METHOD(StatusOr<std::unique_ptr<ReportQueue>>,
              CreateNewQueue,
              (std::unique_ptr<ReportQueueConfiguration> config),
              (override));

  MOCK_METHOD(
      (StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>),
      CreateNewSpeculativeQueue,
      (),
      (override));

  MOCK_METHOD(void, InitOnCompletedCalled, (), (const));

 private:
  // Mock initialization class.
  class MockInitializingContext
      : public ReportQueueProvider::InitializingContext {
   public:
    MockInitializingContext(
        InitCompleteCallback init_complete_cb,
        scoped_refptr<InitializationStateTracker> init_state_tracker,
        MockReportQueueProvider* provider);

   private:
    ~MockInitializingContext() override;

    void OnCompleted() override;
    void OnStart() override;

    MockReportQueueProvider* const provider_;
  };

  scoped_refptr<StorageModuleInterface> storage_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_MOCK_REPORT_QUEUE_PROVIDER_H_
