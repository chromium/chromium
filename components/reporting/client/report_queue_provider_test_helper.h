// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_TEST_HELPER_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_TEST_HELPER_H_

#include <memory>

#include "components/reporting/client/report_queue_provider.h"

namespace reporting {
class MockReportQueueProvider;

namespace test {
class ReportQueueProviderTestHelper {
 public:
  ReportQueueProviderTestHelper();
  ~ReportQueueProviderTestHelper();

  MockReportQueueProvider* mock_provider() const;

 private:
  std::unique_ptr<MockReportQueueProvider> provider_;
};
}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_TEST_HELPER_H_
