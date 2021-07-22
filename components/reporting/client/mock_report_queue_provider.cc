// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/mock_report_queue_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace reporting {

MockReportQueueProvider::MockReportQueueProvider() = default;
MockReportQueueProvider::~MockReportQueueProvider() = default;

ReportQueueProvider::InitializingContext*
MockReportQueueProvider::InstantiateInitializingContext(
    InitCompleteCallback init_complete_cb,
    scoped_refptr<InitializationStateTracker> init_state_tracker) {
  return new MockInitializingContext(std::move(init_complete_cb),
                                     init_state_tracker, this);
}

void MockReportQueueProvider::ExpectCreateNewQueueAndReturnNewMockQueue(
    int times) {
  EXPECT_CALL(*this, CreateNewQueue(_))
      .Times(times)
      .WillRepeatedly([](std::unique_ptr<ReportQueueConfiguration> config) {
        return StatusOr<std::unique_ptr<ReportQueue>>(
            std::make_unique<MockReportQueue>());
      });
}

MockReportQueueProvider::MockInitializingContext::MockInitializingContext(
    InitCompleteCallback init_complete_cb,
    scoped_refptr<InitializationStateTracker> init_state_tracker,
    MockReportQueueProvider* provider)
    : ReportQueueProvider::InitializingContext(std::move(init_complete_cb),
                                               init_state_tracker),
      provider_(provider) {
  DCHECK(provider_);
}

MockReportQueueProvider::MockInitializingContext::~MockInitializingContext() =
    default;

void MockReportQueueProvider::MockInitializingContext::OnStart() {
  // Hand it over to the completion.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&InitializingContext::Complete, base::Unretained(this),
                     Status::StatusOK()));
}

void MockReportQueueProvider::MockInitializingContext::OnCompleted() {
  provider_->InitOnCompletedCalled();
}

}  // namespace reporting
