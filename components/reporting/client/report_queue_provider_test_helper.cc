// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider_test_helper.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace reporting::test {

ReportQueueProviderTestHelper::ReportQueueProviderTestHelper()
    : provider_(
          std::make_unique<::testing::NiceMock<MockReportQueueProvider>>()) {}

ReportQueueProviderTestHelper::~ReportQueueProviderTestHelper() = default;

MockReportQueueProvider* ReportQueueProviderTestHelper::mock_provider() const {
  CHECK(provider_);
  return provider_.get();
}
}  // namespace reporting::test
