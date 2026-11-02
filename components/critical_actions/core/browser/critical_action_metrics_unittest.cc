// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions {
namespace {

TEST(CriticalActionMetricsTest, ActionSourceToHistogramSuffix) {
  EXPECT_EQ(ActionSourceToHistogramSuffix(ActionSource::kPasswordManager),
            "PasswordManager");
  EXPECT_EQ(ActionSourceToHistogramSuffix(ActionSource::kActor), "Actor");
  EXPECT_EQ(ActionSourceToHistogramSuffix(ActionSource::kAutofill), "Autofill");
  EXPECT_EQ(ActionSourceToHistogramSuffix(ActionSource::kUnknown), "Unknown");
}

TEST(CriticalActionMetricsTest, RecordVisitIdResolutionOutcome) {
  base::HistogramTester histogram_tester;
  RecordVisitIdResolutionOutcome(
      ActionSource::kAutofill, VisitIdResolutionOutcome::kEvictedNavigatedAway);

  histogram_tester.ExpectUniqueSample(
      "CriticalActions.VisitIdResolutionOutcome.Autofill",
      VisitIdResolutionOutcome::kEvictedNavigatedAway, 1);
}

TEST(CriticalActionMetricsTest, RecordConversationIdResolutionOutcome) {
  base::HistogramTester histogram_tester;
  RecordConversationIdResolutionOutcome(
      ActionSource::kActor, ConversationIdResolutionOutcome::kMissingTaskId);

  histogram_tester.ExpectUniqueSample(
      "CriticalActions.ConversationIdResolutionOutcome.Actor",
      ConversationIdResolutionOutcome::kMissingTaskId, 1);
}

TEST(CriticalActionMetricsTest, RecordConversationIdDeferredResolutionOutcome) {
  base::HistogramTester histogram_tester;
  RecordConversationIdDeferredResolutionOutcome(
      ActionSource::kPasswordManager,
      ConversationIdDeferredResolutionOutcome::kBackfilled);

  histogram_tester.ExpectUniqueSample(
      "CriticalActions.ConversationIdDeferredResolutionOutcome."
      "PasswordManager",
      ConversationIdDeferredResolutionOutcome::kBackfilled, 1);
}

TEST(CriticalActionMetricsTest, RecordEventLogged) {
  base::HistogramTester histogram_tester;
  RecordEventLogged(ActionSource::kActor, ActionType::kFederatedLogin);

  histogram_tester.ExpectUniqueSample("CriticalActions.EventLogged.Actor",
                                      ActionType::kFederatedLogin, 1);
}

}  // namespace
}  // namespace critical_actions
