// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_provider_test_helper.h"

#include "components/reporting/client/mock_report_queue_provider.h"
#include "components/reporting/client/report_queue_provider.h"

namespace reporting {

namespace report_queue_provider_test_helper {

static MockReportQueueProvider* g_mock_report_queue_provider = nullptr;

void SetForTesting(MockReportQueueProvider* provider) {
  g_mock_report_queue_provider = provider;
}

}  // namespace report_queue_provider_test_helper

// Implementation of the mock report provider for this test helper.
ReportQueueProvider* ReportQueueProvider::GetInstance() {
  return report_queue_provider_test_helper::g_mock_report_queue_provider;
}

}  // namespace reporting
