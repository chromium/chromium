// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_TEST_HELPER_H_
#define COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_TEST_HELPER_H_

namespace reporting {
class MockReportQueueProvider;

namespace report_queue_provider_test_helper {
void SetForTesting(MockReportQueueProvider* provider);
}  // namespace report_queue_provider_test_helper

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_REPORT_QUEUE_PROVIDER_TEST_HELPER_H_
