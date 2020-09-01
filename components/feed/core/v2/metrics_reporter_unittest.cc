// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/metrics_reporter.h"

#include <map>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
using feed::internal::FeedEngagementType;
using feed::internal::FeedUserActionType;
constexpr SurfaceId kSurfaceId = SurfaceId(5);
const base::TimeDelta kEpsilon = base::TimeDelta::FromMilliseconds(1);

class MetricsReporterTest : public testing::Test {
 protected:
  void SetUp() override {
    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());

    // Tests start at the beginning of a day.
    task_environment_.AdvanceClock(
        (base::Time::Now().LocalMidnight() + base::TimeDelta::FromDays(1)) -
        base::Time::Now() + base::TimeDelta::FromSeconds(1));

    RecreateMetricsReporter();
  }
  std::map<FeedEngagementType, int> ReportedEngagementType() {
    std::map<FeedEngagementType, int> result;
    for (const auto& bucket :
         histogram_.GetAllSamples("ContentSuggestions.Feed.EngagementType")) {
      result[static_cast<FeedEngagementType>(bucket.min)] += bucket.count;
    }
    return result;
  }

  void RecreateMetricsReporter() {
    reporter_ = std::make_unique<MetricsReporter>(
        task_environment_.GetMockTickClock(), &profile_prefs_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
  std::unique_ptr<MetricsReporter> reporter_;
  base::HistogramTester histogram_;
  base::UserActionTester user_actions_;
};

TEST_F(MetricsReporterTest, SliceViewedReportsSuggestionShown) {
  reporter_->ContentSliceViewed(kSurfaceId, 5);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Shown", 5, 1);
}

TEST_F(MetricsReporterTest, ScrollingSmall) {
  reporter_->StreamScrolled(100);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, ScrollingCanTriggerEngaged) {
  reporter_->StreamScrolled(161);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, OpeningContentIsInteracting) {
  reporter_->OpenAction(5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, RemovingContentIsInteracting) {
  reporter_->RemoveAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, NotInterestedInIsInteracting) {
  reporter_->NotInterestedInAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, ManageInterestsInIsInteracting) {
  reporter_->ManageInterestsAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, VisitsCanLastMoreThanFiveMinutes) {
  reporter_->StreamScrolled(1);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5) - kEpsilon);
  reporter_->OpenAction(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5) - kEpsilon);
  reporter_->StreamScrolled(1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, NewVisitAfterInactivity) {
  reporter_->OpenAction(0);
  reporter_->StreamScrolled(1);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5) + kEpsilon);
  reporter_->OpenAction(0);
  reporter_->StreamScrolled(1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 2},
      {FeedEngagementType::kFeedInteracted, 2},
      {FeedEngagementType::kFeedEngagedSimple, 2},
      {FeedEngagementType::kFeedScrolled, 2},
  });
  EXPECT_EQ(want, ReportedEngagementType());
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatus) {
  reporter_->OnLoadStream(LoadStreamStatus::kDataInStoreIsStale,
                          LoadStreamStatus::kLoadedFromNetwork,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatusIgnoresNoStatusFromStore) {
  reporter_->OnLoadStream(LoadStreamStatus::kNoStatus,
                          LoadStreamStatus::kLoadedFromNetwork,
                          std::make_unique<LoadLatencyTimes>());

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore", 0);
}

TEST_F(MetricsReporterTest, ReportsLoadStepLatenciesOnFirstView) {
  {
    auto latencies = std::make_unique<LoadLatencyTimes>();
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(150));
    latencies->StepComplete(LoadLatencyTimes::kLoadFromStore);
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(50));
    latencies->StepComplete(LoadLatencyTimes::kUploadActions);
    reporter_->OnLoadStream(LoadStreamStatus::kNoStatus,
                            LoadStreamStatus::kLoadedFromNetwork,
                            std::move(latencies));
  }
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(300));
  reporter_->FeedViewed(kSurfaceId);
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.LoadFromStore",
      base::TimeDelta::FromMilliseconds(150), 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.ActionUpload",
      base::TimeDelta::FromMilliseconds(50), 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.StreamView",
      base::TimeDelta::FromMilliseconds(300), 1);
}

TEST_F(MetricsReporterTest, ReportsLoadMoreStatus) {
  reporter_->OnLoadMore(LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.LoadMore",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, ReportsBackgroundRefreshStatus) {
  reporter_->OnBackgroundRefresh(LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.BackgroundRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, OpenAction) {
  reporter_->OpenAction(5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Open"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOnCard, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewTabAction) {
  reporter_->OpenInNewTabAction(5);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOpenInNewTab, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewIncognitoTabAction) {
  reporter_->OpenInNewIncognitoTabAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab"));
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserActions",
      FeedUserActionType::kTappedOpenInNewIncognitoTab, 1);
  histogram_.ExpectTotalCount("NewTabPage.ContentSuggestions.Opened", 0);
}

TEST_F(MetricsReporterTest, SendFeedbackAction) {
  reporter_->SendFeedbackAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.SendFeedback"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedSendFeedback, 1);
}

TEST_F(MetricsReporterTest, DownloadAction) {
  reporter_->DownloadAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Download"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedDownload, 1);
}

TEST_F(MetricsReporterTest, LearnMoreAction) {
  reporter_->LearnMoreAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.LearnMore"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedLearnMore, 1);
}

TEST_F(MetricsReporterTest, RemoveAction) {
  reporter_->RemoveAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.HideStory"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedHideStory, 1);
}

TEST_F(MetricsReporterTest, NotInterestedInAction) {
  reporter_->NotInterestedInAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.NotInterestedIn"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedNotInterestedIn, 1);
}

TEST_F(MetricsReporterTest, ManageInterestsAction) {
  reporter_->ManageInterestsAction();

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ManageInterests"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedManageInterests, 1);
}

TEST_F(MetricsReporterTest, ContextMenuOpened) {
  reporter_->ContextMenuOpened();

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty, ReportedEngagementType());
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ContextMenu"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kOpenedContextMenu, 1);
}

TEST_F(MetricsReporterTest, SurfaceOpened) {
  reporter_->SurfaceOpened(kSurfaceId);

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty, ReportedEngagementType());
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kOpenedFeedSurface, 1);
}

TEST_F(MetricsReporterTest, OpenFeedSuccessDuration) {
  reporter_->SurfaceOpened(kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration",
      base::TimeDelta::FromSeconds(9), 1);
}

TEST_F(MetricsReporterTest, OpenFeedLoadTimeout) {
  reporter_->SurfaceOpened(kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(16));

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration",
      base::TimeDelta::FromSeconds(15), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, OpenFeedCloseBeforeLoad) {
  reporter_->SurfaceOpened(kSurfaceId);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(14));
  reporter_->SurfaceClosed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration",
      base::TimeDelta::FromSeconds(14), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, OpenCardSuccessDuration) {
  reporter_->OpenAction(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(19));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration",
      base::TimeDelta::FromSeconds(19), 1);
}

TEST_F(MetricsReporterTest, OpenCardTimeout) {
  reporter_->OpenAction(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(21));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, OpenCardFailureTwiceAndThenSucceed) {
  reporter_->OpenAction(0);
  reporter_->OpenAction(1);
  reporter_->OpenAction(2);
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 2);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 1);
}

TEST_F(MetricsReporterTest, OpenCardCloseChromeFailure) {
  reporter_->OpenAction(0);
  reporter_->OnEnterBackground();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedCountsOnlyForegroundTime) {
  reporter_->OpenAction(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  reporter_->OnEnterBackground();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  reporter_->OpenAction(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(3));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(4), 1);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedLimitsIdleTime) {
  reporter_->OpenAction(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(31));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(30), 1);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedIsPerDay) {
  // One interaction every hour for 2 days. Should be reported at 30 seconds per
  // interaction due to the interaction timeout. The 49th |OpenAction()| call
  // triggers reporting the UMA for the previous day.
  for (int i = 0; i < 49; ++i) {
    reporter_->OpenAction(0);
    task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  }

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(30) * 24, 2);
}

TEST_F(MetricsReporterTest, TimeSpentIsPersisted) {
  // Verify that the previous test also works when |MetricsReporter| is
  // destroyed and recreated. The 49th |OpenAction()| call triggers reporting
  // the UMA for the previous day.
  for (int i = 0; i < 49; ++i) {
    reporter_->OpenAction(0);
    task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
    reporter_->OnEnterBackground();
    RecreateMetricsReporter();
  }

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(30) * 24, 2);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedTracksWholeScrollTime) {
  reporter_->StreamScrollStart();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  reporter_->StreamScrolled(1);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::TimeDelta::FromSeconds(3), 1);
}

TEST_F(MetricsReporterTest, TurnOnAction) {
  reporter_->TurnOnAction();
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedTurnOn, 1);
}

TEST_F(MetricsReporterTest, TurnOffAction) {
  reporter_->TurnOffAction();
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedTurnOff, 1);
}

}  // namespace feed
