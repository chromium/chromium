// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

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

TEST(WalletReminderNoticeMetricsTest, LogWalletReminderNoticeShowResult) {
  base::HistogramTester histogram_tester;

  LogWalletReminderNoticeShowResult(WalletReminderNoticeShowResult::kShown);
  histogram_tester.ExpectUniqueSample(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kShown, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownAlreadyAcknowledged);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownAlreadyAcknowledged, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownNetworkOrServerError);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownNetworkOrServerError, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownDueToMandatoryReauth);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownDueToMandatoryReauth, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownDueToVcnEnrollment);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownDueToVcnEnrollment, 1);

  LogWalletReminderNoticeShowResult(
      WalletReminderNoticeShowResult::kNotShownDueToCardOrCvcSave);
  histogram_tester.ExpectBucketCount(
      "Autofill.WalletReminderNotice.ShowResult",
      WalletReminderNoticeShowResult::kNotShownDueToCardOrCvcSave, 1);

  histogram_tester.ExpectTotalCount("Autofill.WalletReminderNotice.ShowResult",
                                    6);
}

}  // namespace autofill::autofill_metrics
