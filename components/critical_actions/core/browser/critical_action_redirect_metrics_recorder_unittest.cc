// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_redirect_metrics_recorder.h"

#include <cstddef>
#include <cstdint>

#include "base/test/metrics/histogram_tester.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace critical_actions {
namespace {

constexpr int kNewChain = ui::PAGE_TRANSITION_LINK |
                          ui::PAGE_TRANSITION_CHAIN_START |
                          ui::PAGE_TRANSITION_CHAIN_END;
constexpr int kServerRedirectChainEnd = ui::PAGE_TRANSITION_LINK |
                                        ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                        ui::PAGE_TRANSITION_CHAIN_END;
constexpr int kClientRedirectChainEnd = ui::PAGE_TRANSITION_LINK |
                                        ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                        ui::PAGE_TRANSITION_CHAIN_END;

constexpr char kServerInRedirectActor[] =
    "CriticalActions.EventLoggedInRedirect.ServerRedirect.Actor";
constexpr char kClientInRedirectActor[] =
    "CriticalActions.EventLoggedInRedirect.ClientRedirect.Actor";

constexpr size_t kCacheCapacity = 2;

history::VisitRow MakeVisit(
    int64_t visit_id,
    int transition,
    int64_t referring_visit = history::kInvalidVisitID) {
  history::VisitRow row;
  row.visit_id = visit_id;
  row.referring_visit = referring_visit;
  row.transition = ui::PageTransitionFromInt(transition);
  return row;
}

class CriticalActionRedirectMetricsRecorderTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
  CriticalActionRedirectMetricsRecorder recorder_{kCacheCapacity};
};

TEST_F(CriticalActionRedirectMetricsRecorderTest,
       RecordsDeferredActionsWhenServerRedirectSeen) {
  recorder_.OnURLVisited(MakeVisit(10, kNewChain));
  recorder_.OnActionResolvedToVisit(10, ActionSource::kActor,
                                    ActionType::kFormFill);
  recorder_.OnActionResolvedToVisit(10, ActionSource::kActor,
                                    ActionType::kFederatedLogin);
  histogram_tester_.ExpectTotalCount(kServerInRedirectActor, 0);

  recorder_.OnURLVisited(MakeVisit(11, kServerRedirectChainEnd, 10));

  histogram_tester_.ExpectTotalCount(kServerInRedirectActor, 2);
  histogram_tester_.ExpectBucketCount(kServerInRedirectActor,
                                      ActionType::kFormFill, 1);
  histogram_tester_.ExpectBucketCount(kServerInRedirectActor,
                                      ActionType::kFederatedLogin, 1);
  histogram_tester_.ExpectTotalCount(kClientInRedirectActor, 0);
}

// V1 -> V2 -> V3 via client redirects. Actions on V1 and V2 are recorded per
// action source; actions on V3 are not.
TEST_F(CriticalActionRedirectMetricsRecorderTest,
       ClientChainRecordsRedirectedVisitsOnly) {
  constexpr int64_t kV1 = 10;
  constexpr int64_t kV2 = 11;
  constexpr int64_t kV3 = 12;
  // A second redirect hop from V2.
  constexpr int64_t kV4 = 13;

  recorder_.OnURLVisited(MakeVisit(kV1, kNewChain));
  recorder_.OnActionResolvedToVisit(kV1, ActionSource::kAutofill,
                                    ActionType::kFormFill);

  recorder_.OnActionResolvedToVisit(kV2, ActionSource::kActor,
                                    ActionType::kFederatedLogin);
  recorder_.OnURLVisited(MakeVisit(kV2, kClientRedirectChainEnd, kV1));
  recorder_.OnActionResolvedToVisit(kV2, ActionSource::kPasswordManager,
                                    ActionType::kGooglePasswordManager);

  recorder_.OnURLVisited(MakeVisit(kV3, kClientRedirectChainEnd, kV2));
  recorder_.OnActionResolvedToVisit(kV3, ActionSource::kActor,
                                    ActionType::kSettingChange);

  // A second redirect hop from V2 must not record its actions again.
  recorder_.OnURLVisited(MakeVisit(kV4, kClientRedirectChainEnd, kV2));

  histogram_tester_.ExpectUniqueSample(
      "CriticalActions.EventLoggedInRedirect.ClientRedirect.Autofill",
      ActionType::kFormFill, 1);
  histogram_tester_.ExpectUniqueSample(kClientInRedirectActor,
                                       ActionType::kFederatedLogin, 1);
  histogram_tester_.ExpectUniqueSample(
      "CriticalActions.EventLoggedInRedirect.ClientRedirect.PasswordManager",
      ActionType::kGooglePasswordManager, 1);
  histogram_tester_.ExpectTotalCount(kServerInRedirectActor, 0);
}

TEST_F(CriticalActionRedirectMetricsRecorderTest,
       IgnoresNewChainAndNonRedirectNavigations) {
  recorder_.OnURLVisited(MakeVisit(10, kNewChain));
  recorder_.OnActionResolvedToVisit(10, ActionSource::kActor,
                                    ActionType::kFormFill);

  // A redirect with PAGE_TRANSITION_CHAIN_START starts a new chain and does not
  // continue the redirect chain from its referring visit.
  recorder_.OnURLVisited(MakeVisit(
      11,
      ui::PAGE_TRANSITION_RELOAD | ui::PAGE_TRANSITION_CLIENT_REDIRECT |
          ui::PAGE_TRANSITION_CHAIN_START | ui::PAGE_TRANSITION_CHAIN_END,
      10));
  // A non-redirect navigation from the visit.
  recorder_.OnURLVisited(MakeVisit(12, kNewChain, 10));

  histogram_tester_.ExpectTotalCount(kServerInRedirectActor, 0);
  histogram_tester_.ExpectTotalCount(kClientInRedirectActor, 0);
}

TEST_F(CriticalActionRedirectMetricsRecorderTest,
       RecordsActionArrivingAfterRedirect) {
  recorder_.OnURLVisited(MakeVisit(10, kNewChain));
  recorder_.OnURLVisited(MakeVisit(11, kClientRedirectChainEnd, 10));

  // A late action for the already-redirected visit is still counted.
  recorder_.OnActionResolvedToVisit(10, ActionSource::kActor,
                                    ActionType::kFormFill);

  histogram_tester_.ExpectUniqueSample(kClientInRedirectActor,
                                       ActionType::kFormFill, 1);
}

TEST_F(CriticalActionRedirectMetricsRecorderTest,
       NotRecordedAfterCacheEviction) {
  for (size_t i = 0; i <= kCacheCapacity; ++i) {
    recorder_.OnActionResolvedToVisit(1000 + i, ActionSource::kActor,
                                      ActionType::kFormFill);
  }

  // The first visit's actions were evicted by the last one.
  recorder_.OnURLVisited(MakeVisit(2000, kClientRedirectChainEnd, 1000));
  histogram_tester_.ExpectTotalCount(kClientInRedirectActor, 0);

  // The most recent visit's actions are still tracked.
  recorder_.OnURLVisited(
      MakeVisit(2001, kClientRedirectChainEnd, 1000 + kCacheCapacity));
  histogram_tester_.ExpectUniqueSample(kClientInRedirectActor,
                                       ActionType::kFormFill, 1);
}

TEST_F(CriticalActionRedirectMetricsRecorderTest,
       ClearDropsDeferredActionsAndKnownRedirects) {
  recorder_.OnActionResolvedToVisit(10, ActionSource::kActor,
                                    ActionType::kFormFill);
  recorder_.OnURLVisited(MakeVisit(21, kClientRedirectChainEnd, 20));

  recorder_.Clear();

  recorder_.OnURLVisited(MakeVisit(11, kClientRedirectChainEnd, 10));
  recorder_.OnActionResolvedToVisit(20, ActionSource::kActor,
                                    ActionType::kFormFill);
  // Only the redirect seen after Clear() is remembered: visit 10 is now known
  // redirected, but its action was dropped. Visit 20's redirect was forgotten.
  histogram_tester_.ExpectTotalCount(kClientInRedirectActor, 0);
}

}  // namespace
}  // namespace critical_actions
