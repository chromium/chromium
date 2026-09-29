// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"

#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

using FlowType =
    payments::RecordLegalReminderAcknowledgmentRequestDetails::FlowType;

std::string_view GetFlowTypeSuffix(FlowType flow_type) {
  switch (flow_type) {
    case FlowType::kChromeDownstream:
      return "ChromeDownstream";
    case FlowType::kWalletPass:
      return "WalletPass";
    case FlowType::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

std::string GetFlowTypeHistogramName(FlowType flow_type) {
  return base::StrCat({"Autofill.WalletReminderNotice.ShowResult.",
                       GetFlowTypeSuffix(flow_type)});
}

TEST(WalletReminderNoticeMetricsTest, LogWalletReminderNoticeInteraction) {
  base::HistogramTester histogram_tester;

  LogWalletReminderNoticeInteraction(
      WalletReminderNoticeInteraction::kAcknowledgedCta);
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.Interaction",
      WalletReminderNoticeInteraction::kAcknowledgedCta, 1);

  LogWalletReminderNoticeInteraction(
      WalletReminderNoticeInteraction::kClickedLink);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.Interaction",
      WalletReminderNoticeInteraction::kClickedLink, 1);

  LogWalletReminderNoticeInteraction(
      WalletReminderNoticeInteraction::kDismissed);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.Interaction",
      WalletReminderNoticeInteraction::kDismissed, 1);

  histogram_tester.ExpectTotalCount("Autofill.WalletReminderNotice.Interaction",
                                    3);
}

class WalletReminderNoticeShowResultMetricsTest
    : public testing::TestWithParam<FlowType> {};

INSTANTIATE_TEST_SUITE_P(All,
                         WalletReminderNoticeShowResultMetricsTest,
                         testing::Values(FlowType::kChromeDownstream,
                                         FlowType::kWalletPass),
                         [](const testing::TestParamInfo<FlowType>& info) {
                           return std::string(GetFlowTypeSuffix(info.param));
                         });

TEST_P(WalletReminderNoticeShowResultMetricsTest,
       LogWalletReminderNoticeShowResult) {
  base::HistogramTester histogram_tester;
  const FlowType flow_type = GetParam();
  const std::string flow_histogram = GetFlowTypeHistogramName(flow_type);

  LogWalletReminderNoticeShowResult(WalletReminderNoticeShowResult::kShown,
                                    flow_type);
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      flow_histogram, WalletReminderNoticeShowResult::kShown, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToServer,
      flow_type);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToServer,
      1);
  histogram_tester.ExpectBucketCount(
      flow_histogram,
      WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToServer,
      1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToPref,
      flow_type);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToPref,
      1);
  histogram_tester.ExpectBucketCount(
      flow_histogram,
      WalletReminderNoticeShowResult::
          kNotShownAlreadyAcknowledgedAccordingToPref,
      1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownNetworkOrServerError, flow_type);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownNetworkOrServerError, 1);
  histogram_tester.ExpectBucketCount(
      flow_histogram,
      WalletReminderNoticeShowResult::kNotShownNetworkOrServerError, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownDueToMandatoryReauth, flow_type);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownDueToMandatoryReauth, 1);
  histogram_tester.ExpectBucketCount(
      flow_histogram,
      WalletReminderNoticeShowResult::kNotShownDueToMandatoryReauth, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownDueToVcnEnrollment, flow_type);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownDueToVcnEnrollment, 1);
  histogram_tester.ExpectBucketCount(
      flow_histogram,
      WalletReminderNoticeShowResult::kNotShownDueToVcnEnrollment, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownDueToCardOrCvcSave, flow_type);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownDueToCardOrCvcSave, 1);
  histogram_tester.ExpectBucketCount(
      flow_histogram,
      WalletReminderNoticeShowResult::kNotShownDueToCardOrCvcSave, 1);

  histogram_tester.ExpectTotalCount("Autofill.WalletReminderNotice.ShowResult",
                                    7);
  histogram_tester.ExpectTotalCount(flow_histogram, 7);
}

}  // namespace

}  // namespace autofill::autofill_metrics
