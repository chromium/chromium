// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/metrics_reporter.h"

#include <map>
#include <memory>
#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
// Using the "unspecified" stream type to represent the combined streams when
// checking engagement metrics.
StreamType kCombinedStreams = StreamType();

constexpr SurfaceId kSurfaceId = SurfaceId(5);
const base::TimeDelta kEpsilon = base::Milliseconds(1);
const int kSubscriptionCount = 42;
const ContentStats kContentStats = {
    /*card_count=*/12,
    /*total_content_frame_size_bytes=*/100 * 1024,
    /*shared_state_size=*/200 * 1024};

MetricsReporter::LoadStreamResultSummary NetworkLoadResults() {
  MetricsReporter::LoadStreamResultSummary summary;
  summary.load_from_store_status = LoadStreamStatus::kDataInStoreIsStale;
  summary.final_status = LoadStreamStatus::kLoadedFromNetwork;
  summary.is_initial_load = true;
  summary.loaded_new_content_from_network = true;
  summary.stored_content_age = base::Days(5);
  summary.content_order = ContentOrder::kGrouped;
  return summary;
}

MetricsReporter::LoadStreamResultSummary LoadFailureResults(
    LoadStreamStatus final_status) {
  MetricsReporter::LoadStreamResultSummary summary;
  summary.load_from_store_status = LoadStreamStatus::kDataInStoreIsStale;
  summary.final_status = final_status;
  summary.is_initial_load = true;
  summary.loaded_new_content_from_network = false;
  summary.stored_content_age = base::Days(5);
  summary.content_order = ContentOrder::kGrouped;
  return summary;
}

class MetricsReporterTest : public testing::Test, MetricsReporter::Delegate {
 protected:
  void SetUp() override {
    feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
    feed::RegisterProfilePrefs(profile_prefs_.registry());

    // Tests start at the beginning of a day.
    task_environment_.AdvanceClock(
        (base::Time::Now().LocalMidnight() + base::Days(1)) -
        base::Time::Now() + base::Seconds(1));

    test_content_order_ = ContentOrder::kGrouped;

    RecreateMetricsReporter();
  }
  std::map<FeedEngagementType, int> ReportedEngagementType(
      const StreamType& stream_type) {
    std::map<FeedEngagementType, int> result;
    const char* histogram_name;
    switch (stream_type.GetKind()) {
      case StreamKind::kForYou:
        histogram_name = "ContentSuggestions.Feed.EngagementType";
        break;
      case StreamKind::kFollowing:
        histogram_name = "ContentSuggestions.Feed.WebFeed.EngagementType";
        break;
      case StreamKind::kUnknown:
        histogram_name = "ContentSuggestions.Feed.AllFeeds.EngagementType";
        break;
      case StreamKind::kSingleWebFeed:
        histogram_name = "ContentSuggestions.Feed.SingleWebFeed.EngagementType";
        break;
      case StreamKind::kSupervisedUser:
        histogram_name =
            "ContentSuggestions.Feed.SupervisedFeed.EngagementType";
        break;
    }
    for (const auto& bucket : histogram_.GetAllSamples(histogram_name)) {
      result[static_cast<FeedEngagementType>(bucket.min)] += bucket.count;
    }
    return result;
  }

  void RecreateMetricsReporter() {
    reporter_ = std::make_unique<MetricsReporter>(&profile_prefs_);
    reporter_->Initialize(this);
  }

 protected:
  // MetricsReporter::Delegate
  void SubscribedWebFeedCount(base::OnceCallback<void(int)> callback) override {
    std::move(callback).Run(kSubscriptionCount);
  }
  void RegisterFeedUserSettingsFieldTrial(std::string_view group) override {
    register_feed_user_settings_field_trial_calls_.push_back(
        static_cast<std::string>(group));
  }
  ContentOrder GetContentOrder(const StreamType& stream_type) const override {
    return test_content_order_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
  std::unique_ptr<MetricsReporter> reporter_;
  base::HistogramTester histogram_;
  base::UserActionTester user_actions_;
  std::vector<std::string> register_feed_user_settings_field_trial_calls_;
  base::test::ScopedFeatureList feature_list_;
  ContentOrder test_content_order_;
};

TEST_F(MetricsReporterTest, SliceViewedReportsSuggestionShown) {
  reporter_->ContentSliceViewed(StreamType(StreamKind::kForYou), 5, 7);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Shown", 5, 1);
  reporter_->ContentSliceViewed(StreamType(StreamKind::kFollowing), 5, 7);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.WebFeed.Shown", 5, 1);
  histogram_.ExpectTotalCount("ContentSuggestions.Feed.ReachedEndOfFeed", 0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.ReachedEndOfFeed", 0);
  reporter_->ContentSliceViewed(StreamType(StreamKind::kSingleWebFeed), 5, 7);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.SingleWebFeed.Shown",
                                5, 1);
}

TEST_F(MetricsReporterTest, LastSliceViewedReportsReachedEndOfFeed) {
  reporter_->ContentSliceViewed(StreamType(StreamKind::kForYou), 5, 6);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.ReachedEndOfFeed", 6,
                                1);
}

TEST_F(MetricsReporterTest, WebFeed_LastSliceViewedReportsReachedEndOfFeed) {
  reporter_->ContentSliceViewed(StreamType(StreamKind::kFollowing), 5, 6);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.ReachedEndOfFeed", 6, 1);
}

TEST_F(MetricsReporterTest, ScrollingSmall) {
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 100);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  histogram_.ExpectTotalCount("ContentSuggestions.Feed.FollowCount.Engaged2",
                              0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2", 0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2", 0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged", 0);
}

TEST_F(MetricsReporterTest, ScrollingCanTriggerEngaged) {
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 161);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.FollowCount.Engaged2",
                                kSubscriptionCount, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2", 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged",
      test_content_order_, 0);
}

TEST_F(MetricsReporterTest, WebFeedEngagementRecordsSortType) {
  test_content_order_ = ContentOrder::kReverseChron;
  reporter_->StreamScrolled(StreamType(StreamKind::kFollowing), 161);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kFollowing)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  histogram_.ExpectTotalCount("ContentSuggestions.Feed.FollowCount.Engaged2",
                              0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged",
      test_content_order_, 1);
}

TEST_F(MetricsReporterTest, OpeningContentIsInteracting) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 5,
                        OpenActionType::kDefault);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
}

TEST_F(MetricsReporterTest, RemovingContentIsInteracting) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedHideStory);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
}

TEST_F(MetricsReporterTest, NotInterestedInIsInteracting) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedNotInterestedIn);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
}

TEST_F(MetricsReporterTest, ManageInterestsInIsInteracting) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedManageInterests);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
}

TEST_F(MetricsReporterTest, VisitsCanLastMoreThanFiveMinutes) {
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  task_environment_.FastForwardBy(base::Minutes(5) - kEpsilon);
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Minutes(5) - kEpsilon);
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedScrolled, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
}

TEST_F(MetricsReporterTest, NewVisitAfterInactivity) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  task_environment_.FastForwardBy(base::Minutes(5) + kEpsilon);
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 2},
      {FeedEngagementType::kFeedInteracted, 2},
      {FeedEngagementType::kFeedEngagedSimple, 2},
      {FeedEngagementType::kFeedScrolled, 2},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
}

TEST_F(MetricsReporterTest, InteractedWithBothFeeds) {
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  reporter_->StreamScrolled(StreamType(StreamKind::kFollowing), 1);
  reporter_->OpenAction(StreamType(StreamKind::kFollowing), 0,
                        OpenActionType::kDefault);

  std::map<FeedEngagementType, int> want_1({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
      {FeedEngagementType::kFeedScrolled, 1},
  });
  EXPECT_EQ(want_1, ReportedEngagementType(StreamType(StreamKind::kFollowing)));
  EXPECT_EQ(want_1, ReportedEngagementType(StreamType(StreamKind::kForYou)));

  std::map<FeedEngagementType, int> want_1c({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 2},
      {FeedEngagementType::kFeedEngagedSimple, 1},
      {FeedEngagementType::kFeedScrolled, 1},
  });
  EXPECT_EQ(want_1c, ReportedEngagementType(kCombinedStreams));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.FollowCount.Engaged2",
                                kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged",
      test_content_order_, 1);

  task_environment_.FastForwardBy(base::Minutes(5) + kEpsilon);
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);

  EXPECT_EQ(want_1, ReportedEngagementType(StreamType(StreamKind::kFollowing)));

  std::map<FeedEngagementType, int> want_2({
      {FeedEngagementType::kFeedEngaged, 2},
      {FeedEngagementType::kFeedInteracted, 2},
      {FeedEngagementType::kFeedEngagedSimple, 2},
      {FeedEngagementType::kFeedScrolled, 2},
  });
  EXPECT_EQ(want_2, ReportedEngagementType(StreamType(StreamKind::kForYou)));

  std::map<FeedEngagementType, int> want_2c({
      {FeedEngagementType::kFeedEngaged, 2},
      {FeedEngagementType::kFeedInteracted, 3},
      {FeedEngagementType::kFeedEngagedSimple, 2},
      {FeedEngagementType::kFeedScrolled, 2},
  });
  EXPECT_EQ(want_2c, ReportedEngagementType(kCombinedStreams));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.FollowCount.Engaged2",
                                kSubscriptionCount, 2);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2",
      kSubscriptionCount, 2);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged",
      test_content_order_, 1);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatus) {
  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), NetworkLoadResults(),
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.LoadedCardCount",
                                kContentStats.card_count, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.StreamContentSizeKB",
      kContentStats.total_content_frame_size_bytes / 1024, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.SharedStateSizeKB",
                                kContentStats.shared_state_size / 1024, 1);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatusWhenDisabled) {
  feedstore::Metadata::StreamMetadata stream_metadata;
  reporter_->OnLoadStream(
      StreamType(StreamKind::kForYou),
      LoadFailureResults(LoadStreamStatus::kLoadNotAllowedDisabled),
      kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadNotAllowedDisabled, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
}

TEST_F(MetricsReporterTest, WebFeed_ReportsLoadStreamStatus) {
  reporter_->OnLoadStream(StreamType(StreamKind::kFollowing),
                          NetworkLoadResults(), kContentStats,
                          std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadedCardCount",
      kContentStats.card_count, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadedCardCount.Grouped",
      kContentStats.card_count, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.LoadedCardCount.ReverseChron", 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.ContentShown",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.StreamContentSizeKB",
      kContentStats.total_content_frame_size_bytes / 1024, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.SharedStateSizeKB",
      kContentStats.shared_state_size / 1024, 1);
}

TEST_F(MetricsReporterTest, WebFeed_ReportsLoadStreamStatus_ReverseChron) {
  feedstore::Metadata::StreamMetadata stream_metadata;
  MetricsReporter::LoadStreamResultSummary result_summary =
      NetworkLoadResults();
  result_summary.content_order = ContentOrder::kReverseChron;
  reporter_->OnLoadStream(StreamType(StreamKind::kFollowing), result_summary,
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadedCardCount",
      kContentStats.card_count, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadedCardCount.ReverseChron",
      kContentStats.card_count, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.LoadedCardCount.Grouped", 0);
}

TEST_F(MetricsReporterTest, WebFeed_ReportsNoContentShown) {
  reporter_->OnLoadStream(StreamType(StreamKind::kFollowing),
                          NetworkLoadResults(), ContentStats(),
                          std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.NoContentShown",
      kSubscriptionCount, 1);
}

TEST_F(MetricsReporterTest, OnLoadStreamDoesNotReportLoadedCardCountOnFailure) {
  MetricsReporter::LoadStreamResultSummary result_summary =
      LoadFailureResults(LoadStreamStatus::kDataInStoreIsExpired);

  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), result_summary,
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectTotalCount("ContentSuggestions.Feed.LoadedCardCount", 0);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatusForManualRefresh) {
  MetricsReporter::LoadStreamResultSummary result_summary =
      NetworkLoadResults();
  result_summary.is_initial_load = false;
  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), result_summary,
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.ManualRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest,
       SingleWebFeed_ReportsLoadStreamStatusForManualRefresh) {
  MetricsReporter::LoadStreamResultSummary result_summary =
      NetworkLoadResults();
  result_summary.is_initial_load = false;
  reporter_->OnLoadStream(StreamType(StreamKind::kSingleWebFeed),
                          result_summary, kContentStats,
                          std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.LoadStreamStatus.ManualRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, ReportsLoadStreamStatusIgnoresNoStatusFromStore) {
  MetricsReporter::LoadStreamResultSummary result_summary =
      NetworkLoadResults();
  result_summary.load_from_store_status = LoadStreamStatus::kNoStatus;
  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), result_summary,
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.LoadStreamStatus.InitialFromStore", 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.UserRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 0);
}

TEST_F(MetricsReporterTest, ReportsContentAgeBlockingRefresh) {
  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), NetworkLoadResults(),
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh", base::Days(5),
      1);
}

TEST_F(MetricsReporterTest, ReportsContentAgeNoRefresh) {
  MetricsReporter::LoadStreamResultSummary result_summary =
      NetworkLoadResults();
  result_summary.loaded_new_content_from_network = false;
  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), result_summary,
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed", base::Days(5),
      1);
}

TEST_F(MetricsReporterTest, DoNotReportContentAgeWhenNotPositive) {
  MetricsReporter::LoadStreamResultSummary result_summary =
      LoadFailureResults(LoadStreamStatus::kLoadedFromStore);
  result_summary.stored_content_age = -base::Seconds(1);
  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), result_summary,
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  result_summary.stored_content_age = base::TimeDelta();
  reporter_->OnLoadStream(StreamType(StreamKind::kForYou), result_summary,
                          kContentStats, std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed", 0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh", 0);
}

TEST_F(MetricsReporterTest, ReportsLoadStepLatenciesOnFirstView) {
  feedstore::Metadata::StreamMetadata stream_metadata;
  {
    auto latencies = std::make_unique<LoadLatencyTimes>();
    task_environment_.FastForwardBy(base::Milliseconds(150));
    latencies->StepComplete(LoadLatencyTimes::kLoadFromStore);
    task_environment_.FastForwardBy(base::Milliseconds(50));
    latencies->StepComplete(LoadLatencyTimes::kUploadActions);
    MetricsReporter::LoadStreamResultSummary result_summary =
        NetworkLoadResults();
    result_summary.stored_content_age = base::TimeDelta();
    reporter_->OnLoadStream(StreamType(StreamKind::kForYou), result_summary,
                            kContentStats, std::move(latencies));
  }
  task_environment_.FastForwardBy(base::Milliseconds(300));
  reporter_->FeedViewed(kSurfaceId);
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.LoadFromStore",
      base::Milliseconds(150), 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.ActionUpload",
      base::Milliseconds(50), 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.LoadStepLatency.StreamView",
      base::Milliseconds(300), 1);
}

TEST_F(MetricsReporterTest, ReportsLoadMoreStatus) {
  reporter_->OnLoadMore(StreamType(StreamKind::kForYou),
                        LoadStreamStatus::kLoadedFromNetwork, kContentStats);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.LoadMore",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, ReportsBackgroundRefreshStatus) {
  reporter_->OnBackgroundRefresh(StreamType(StreamKind::kForYou),
                                 LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.LoadStreamStatus.BackgroundRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, WebFeed_ReportsBackgroundRefreshStatus) {
  reporter_->OnBackgroundRefresh(StreamType(StreamKind::kFollowing),
                                 LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.LoadStreamStatus.BackgroundRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, SingleWebFeed_ReportsBackgroundRefreshStatus) {
  reporter_->OnBackgroundRefresh(StreamType(StreamKind::kSingleWebFeed),
                                 LoadStreamStatus::kLoadedFromNetwork);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.LoadStreamStatus."
      "BackgroundRefresh",
      LoadStreamStatus::kLoadedFromNetwork, 1);
}

TEST_F(MetricsReporterTest, OpenAction) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 5,
                        OpenActionType::kDefault);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Open"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOnCard, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.FollowCount.Engaged2",
                                kSubscriptionCount, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2", 0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged",
      test_content_order_, 0);
}

TEST_F(MetricsReporterTest, OpenActionWebFeed) {
  reporter_->OpenAction(StreamType(StreamKind::kFollowing), 5,
                        OpenActionType::kDefault);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kFollowing)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Open"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOnCard, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.WebFeed.Opened", 5, 1);
  histogram_.ExpectTotalCount("ContentSuggestions.Feed.FollowCount.Engaged2",
                              0);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2",
      kSubscriptionCount, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.SortTypeWhenEngaged",
      test_content_order_, 1);
}

TEST_F(MetricsReporterTest, OpenInNewTabAction) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 5,
                        OpenActionType::kNewTab);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOpenInNewTab, 1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewTabInGroupAction) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 5,
                        OpenActionType::kNewTabInGroup);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTabInGroup"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOpenInNewTabInGroup,
                                1);
  histogram_.ExpectUniqueSample("NewTabPage.ContentSuggestions.Opened", 5, 1);
}

TEST_F(MetricsReporterTest, OpenInNewIncognitoTabAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedOpenInNewIncognitoTab);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));

  std::map<FeedEngagementType, int> want_allfeeds({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
      {FeedEngagementType::kGoodVisit, 1},
  });
  EXPECT_EQ(want_allfeeds, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab"));
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserActions",
      FeedUserActionType::kTappedOpenInNewIncognitoTab, 1);
  histogram_.ExpectTotalCount("NewTabPage.ContentSuggestions.Opened", 0);
}

TEST_F(MetricsReporterTest, SendFeedbackAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedSendFeedback);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.SendFeedback"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedSendFeedback, 1);
}

TEST_F(MetricsReporterTest, DownloadAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedDownload);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));

  std::map<FeedEngagementType, int> want_allfeeds({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
      {FeedEngagementType::kGoodVisit, 1},
  });
  EXPECT_EQ(want_allfeeds, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Download"));
  histogram_.ExpectBucketCount("ContentSuggestions.Feed.UserActions",
                               FeedUserActionType::kTappedDownload, 1);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, LearnMoreAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedLearnMore);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.LearnMore"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedLearnMore, 1);
}

TEST_F(MetricsReporterTest, RemoveAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedHideStory);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.HideStory"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedHideStory, 1);
}

TEST_F(MetricsReporterTest, NotInterestedInAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedNotInterestedIn);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.NotInterestedIn"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedNotInterestedIn, 1);
}

TEST_F(MetricsReporterTest, ManageInterestsAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedManageInterests);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want, ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ManageInterests"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedManageInterests, 1);
}

TEST_F(MetricsReporterTest, ContextMenuOpened) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kOpenedContextMenu);

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty,
            ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want_empty, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.ContextMenu"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kOpenedContextMenu, 1);
}

TEST_F(MetricsReporterTest, SurfaceOpened) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kForYou), kSurfaceId);

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty,
            ReportedEngagementType(StreamType(StreamKind::kForYou)));
  EXPECT_EQ(want_empty, ReportedEngagementType(kCombinedStreams));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kOpenedFeedSurface, 1);
}

TEST_F(MetricsReporterTest, SurfaceOpenedSingleWebFeedEntryPoint) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kSingleWebFeed), kSurfaceId,
                           SingleWebFeedEntryPoint::kMenu);

  std::map<FeedEngagementType, int> want_empty;
  EXPECT_EQ(want_empty,
            ReportedEngagementType(StreamType(StreamKind::kSingleWebFeed)));
  EXPECT_EQ(want_empty, ReportedEngagementType(kCombinedStreams));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kOpenedFeedSurface, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.SingleWebFeed.EntryPoint",
                                SingleWebFeedEntryPoint::kMenu, 1);
}

TEST_F(MetricsReporterTest, OpenFeedSuccessDuration) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kForYou), kSurfaceId);
  task_environment_.FastForwardBy(base::Seconds(9));
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration",
      base::Seconds(9), 1);
}

TEST_F(MetricsReporterTest, WebFeed_OpenFeedSuccessDuration) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kFollowing), kSurfaceId);
  task_environment_.FastForwardBy(base::Seconds(9));
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.SuccessDuration",
      base::Seconds(9), 1);
}
TEST_F(MetricsReporterTest, SingleWebFeed_OpenFeedSuccessDuration) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kSingleWebFeed), kSurfaceId);
  task_environment_.FastForwardBy(base::Seconds(9));
  reporter_->FeedViewed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SingleWebFeed."
      "SuccessDuration",
      base::Seconds(9), 1);
}

TEST_F(MetricsReporterTest, OpenFeedLoadTimeout) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kForYou), kSurfaceId);
  task_environment_.FastForwardBy(base::Seconds(16));

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration",
      base::Seconds(15), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, WebFeed_OpenFeedLoadTimeout) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kFollowing), kSurfaceId);
  task_environment_.FastForwardBy(base::Seconds(16));

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.FailureDuration",
      base::Seconds(15), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.SuccessDuration",
      0);
}

TEST_F(MetricsReporterTest, OpenFeedCloseBeforeLoad) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kForYou), kSurfaceId);
  task_environment_.FastForwardBy(base::Seconds(14));
  reporter_->SurfaceClosed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.FailureDuration",
      base::Seconds(14), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, WebFeed_OpenFeedCloseBeforeLoad) {
  reporter_->SurfaceOpened(StreamType(StreamKind::kFollowing), kSurfaceId);
  task_environment_.FastForwardBy(base::Seconds(14));
  reporter_->SurfaceClosed(kSurfaceId);

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.FailureDuration",
      base::Seconds(14), 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenFeed.WebFeed.SuccessDuration",
      0);
}

TEST_F(MetricsReporterTest, OpenCardSuccessDuration) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Seconds(19));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration",
      base::Seconds(19), 1);
}

TEST_F(MetricsReporterTest, WebFeed_OpenCardSuccessDuration) {
  reporter_->OpenAction(StreamType(StreamKind::kFollowing), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Seconds(19));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.SuccessDuration",
      base::Seconds(19), 1);
}

TEST_F(MetricsReporterTest, OpenCardTimeout) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Seconds(21));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, WebFeed_OpenCardTimeout) {
  reporter_->OpenAction(StreamType(StreamKind::kFollowing), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Seconds(21));
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.SuccessDuration",
      0);
}

TEST_F(MetricsReporterTest, OpenCardFailureTwiceAndThenSucceed) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 1,
                        OpenActionType::kDefault);
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 2,
                        OpenActionType::kDefault);
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 2);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 1);
}

TEST_F(MetricsReporterTest, WebFeed_OpenCardFailureTwiceAndThenSucceed) {
  reporter_->OpenAction(StreamType(StreamKind::kFollowing), 0,
                        OpenActionType::kDefault);
  reporter_->OpenAction(StreamType(StreamKind::kFollowing), 1,
                        OpenActionType::kDefault);
  reporter_->OpenAction(StreamType(StreamKind::kFollowing), 2,
                        OpenActionType::kDefault);
  reporter_->PageLoaded();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.Failure", 1, 2);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.WebFeed.SuccessDuration",
      1);
}

TEST_F(MetricsReporterTest, OpenCardCloseChromeFailure) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  reporter_->OnEnterBackground();

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserJourney.OpenCard.Failure", 1, 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.UserJourney.OpenCard.SuccessDuration", 0);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedCountsOnlyForegroundTime) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Seconds(1));
  reporter_->OnEnterBackground();
  task_environment_.FastForwardBy(base::Seconds(2));
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Seconds(3));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::Days(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::Seconds(4), 1);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedLimitsIdleTime) {
  reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                        OpenActionType::kDefault);
  task_environment_.FastForwardBy(base::Seconds(31));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::Days(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::Seconds(30), 1);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedIsPerDay) {
  // One interaction every hour for 2 days. Should be reported at 30 seconds per
  // interaction due to the interaction timeout. The 49th |OpenAction()| call
  // triggers reporting the UMA for the previous day.
  for (int i = 0; i < 49; ++i) {
    reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                          OpenActionType::kDefault);
    task_environment_.FastForwardBy(base::Hours(1));
  }

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::Seconds(30) * 24, 2);
}

TEST_F(MetricsReporterTest, TimeSpentIsPersisted) {
  // Verify that the previous test also works when |MetricsReporter| is
  // destroyed and recreated. The 49th |OpenAction()| call triggers reporting
  // the UMA for the previous day.
  for (int i = 0; i < 49; ++i) {
    reporter_->OpenAction(StreamType(StreamKind::kForYou), 0,
                          OpenActionType::kDefault);
    task_environment_.FastForwardBy(base::Hours(1));
    reporter_->OnEnterBackground();
    RecreateMetricsReporter();
  }

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::Seconds(30) * 24, 2);
}

TEST_F(MetricsReporterTest, TimeSpentInFeedTracksWholeScrollTime) {
  reporter_->StreamScrollStart();
  task_environment_.FastForwardBy(base::Seconds(2));
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  task_environment_.FastForwardBy(base::Seconds(1));
  reporter_->OnEnterBackground();

  // Trigger reporting the persistent metrics the next day.
  task_environment_.FastForwardBy(base::Days(1));
  RecreateMetricsReporter();

  histogram_.ExpectUniqueTimeSample("ContentSuggestions.Feed.TimeSpentInFeed",
                                    base::Seconds(3), 1);
}

TEST_F(MetricsReporterTest, TurnOnAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedTurnOn);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedTurnOn, 1);
}

TEST_F(MetricsReporterTest, TurnOffAction) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedTurnOff);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedTurnOff, 1);
}

TEST_F(MetricsReporterTest, NetworkRequestCompleteReportsUma) {
  NetworkResponseInfo response_info;
  response_info.status_code = 200;
  response_info.fetch_duration = base::Seconds(2);
  response_info.encoded_size_bytes = 123 * 1024;

  MetricsReporter::NetworkRequestComplete(NetworkRequestType::kListWebFeeds,
                                          response_info);

  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.Network.ResponseStatus.ListFollowedWebFeeds",
      200, 1);
  histogram_.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.Network.Duration.ListFollowedWebFeeds",
      base::Seconds(2), 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.Network.CompressedResponseSizeKB."
      "ListFollowedWebFeeds",
      123, 1);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_FeedNotEnabled) {
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/false,
                                   /*isSignedIn=*/false,
                                   /*isEnabled=*/false, feedstore::Metadata());
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kFeedNotEnabled, 1);
  EXPECT_EQ(std::vector<std::string>({"FeedNotEnabled"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_FeedNotEnabledByPolicy) {
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/false,
                                   /*isFeedVisible=*/false,
                                   /*isSignedIn=*/false,
                                   /*isEnabled=*/true, feedstore::Metadata());
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kFeedNotEnabledByPolicy,
                                1);
  EXPECT_EQ(std::vector<std::string>({"FeedNotEnabledByPolicy"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_FeedNotVisible_SignedOut) {
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/false,
                                   /*isSignedIn=*/false,
                                   /*isEnabled=*/true, feedstore::Metadata());
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kFeedNotVisibleSignedOut,
                                1);
  EXPECT_EQ(std::vector<std::string>({"FeedNotVisibleSignedOut"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_FeedNotVisible_SignedIn) {
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/false,
                                   /*isSignedIn=*/true,
                                   /*isEnabled=*/true, feedstore::Metadata());
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kFeedNotVisibleSignedIn,
                                1);
  EXPECT_EQ(std::vector<std::string>({"FeedNotVisibleSignedIn"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_EnabledSignedOut) {
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/true,
                                   /*isSignedIn=*/false,
                                   /*isEnabled=*/true, feedstore::Metadata());
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedOut, 1);
  EXPECT_EQ(std::vector<std::string>({"SignedOut"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_WaaOffDpOff) {
  feedstore::Metadata metadata;
  // Content age is within kUserSettingsMaxAge.
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now() - kUserSettingsMaxAge));
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/true,
                                   /*isSignedIn=*/true,
                                   /*isEnabled=*/true, metadata);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInWaaOffDpOff, 1);
  EXPECT_EQ(std::vector<std::string>({"SignedInWaaOffDpOff"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_WaaOnDpOff) {
  feedstore::Metadata metadata;
  // Content age is within kUserSettingsMaxAge.
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now()));
  metadata.set_web_and_app_activity_enabled(true);
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/true,
                                   /*isSignedIn=*/true,
                                   /*isEnabled=*/true, metadata);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInWaaOnDpOff, 1);
  EXPECT_EQ(std::vector<std::string>({"SignedInWaaOnDpOff"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_WaaOffDpOn) {
  feedstore::Metadata metadata;
  // Only the first stream has age less than kUserSettingsMaxAge.
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now()));
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now() - kUserSettingsMaxAge -
                                   base::Seconds(1)));
  metadata.set_discover_personalization_enabled(true);
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/true,
                                   /*isSignedIn=*/true,
                                   /*isEnabled=*/true, metadata);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInWaaOffDpOn, 1);
  EXPECT_EQ(std::vector<std::string>({"SignedInWaaOffDpOn"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_WaaOnDpOn) {
  feedstore::Metadata metadata;
  // Only the second stream has age less than kUserSettingsMaxAge.
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now() - kUserSettingsMaxAge -
                                   base::Seconds(1)));
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now()));
  metadata.set_discover_personalization_enabled(true);
  metadata.set_web_and_app_activity_enabled(true);
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/true,
                                   /*isSignedIn=*/true,
                                   /*isEnabled=*/true, metadata);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInWaaOnDpOn, 1);
  EXPECT_EQ(std::vector<std::string>({"SignedInWaaOnDpOn"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_FeedDataTooOld) {
  feedstore::Metadata metadata;
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now() - kUserSettingsMaxAge -
                                   base::Seconds(1)));
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/true,
                                   /*isSignedIn=*/true,
                                   /*isEnabled=*/true, metadata);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInNoRecentData, 1);
  EXPECT_EQ(std::vector<std::string>({"SignedInNoRecentData"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, UserSettingsOnStart_FeedDataFromFuture) {
  feedstore::Metadata metadata;
  metadata.add_stream_metadata()->set_last_fetch_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now() + base::Seconds(1)));
  reporter_->OnMetadataInitialized(/*isEnabledByEnterprisePolicy=*/true,
                                   /*isFeedVisible=*/true,
                                   /*isSignedIn=*/true,
                                   /*isEnabled=*/true, metadata);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInNoRecentData, 1);
  EXPECT_EQ(std::vector<std::string>({"SignedInNoRecentData"}),
            register_feed_user_settings_field_trial_calls_);
}

TEST_F(MetricsReporterTest, ReportInfoCard) {
  reporter_->OnInfoCardTrackViewStarted(StreamType(StreamKind::kForYou), 0);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.InfoCard.Started", 0,
                                1);

  reporter_->OnInfoCardViewed(StreamType(StreamKind::kForYou), 0);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.InfoCard.Viewed", 0,
                                1);

  reporter_->OnInfoCardClicked(StreamType(StreamKind::kFollowing), 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.InfoCard.Clicked", 1, 1);

  reporter_->OnInfoCardDismissedExplicitly(StreamType(StreamKind::kForYou), 0);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.InfoCard.Dismissed", 0,
                                1);

  reporter_->OnInfoCardStateReset(StreamType(StreamKind::kFollowing), 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.InfoCard.Reset", 1, 1);

  reporter_->OnInfoCardClicked(StreamType(StreamKind::kSingleWebFeed), 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.InfoCard.Clicked", 1, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_Scroll_GoodTimeSpentInFeed) {
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(30));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 0);

  // Passing a minute in the feed should log a Good Visit since a scroll
  // happened.
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(30));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_GoodTimeSpentInFeed_Scroll) {
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(30));
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(30));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 0);

  // Scrolling should log a good visit since the user already spent a minute in
  // the feed.
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_SmallTimesDroppped) {
  // Reach 59.9 seconds and a scroll.
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(30));
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(29) + base::Milliseconds(900));
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 0);

  // Ignore less than half a second.
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Milliseconds(200));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 0);

  // More than half a second counts.
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Milliseconds(501));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_LargeTimesCapped) {
  reporter_->StreamScrolled(StreamType(StreamKind::kForYou), 1);
  // Capped to 30 seconds.
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(61));
  // 59 seconds so far.
  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(29));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 0);

  reporter_->ReportStableContentSliceVisibilityTimeForGoodVisits(
      base::Seconds(2));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_GoodExplicitInteraction_Share) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kShare);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_GoodExplicitInteraction_Download) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedDownload);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest,
       GoodVisit_GoodExplicitInteraction_AddToReadingList) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedAddToReadingList);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_GoodExplicitInteraction_AddToReadLater) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kAddedToReadLater);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_GoodExplicitInteraction_Follow) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kTappedFollowButton);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_LongClick) {
  reporter_->OpenVisitComplete(base::Seconds(9));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 0);

  reporter_->OpenVisitComplete(base::Seconds(10));
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, GoodVisit_OnlyLoggedOncePerVisit) {
  // Start with a good visit.
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kShare);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);

  // Things that would normally count as a good visit shouldn't count
  // until the next visit.
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kShare);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);

  task_environment_.FastForwardBy(base::Minutes(5));
  // A new visit has started and can be a good visit.
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kShare);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 2);
}

TEST_F(MetricsReporterTest, GoodVisitStateIsPersistent) {
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kShare);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
  reporter_->OnEnterBackground();
  RecreateMetricsReporter();

  // This one shouldn't report a good visit.
  reporter_->OtherUserAction(StreamType(StreamKind::kForYou),
                             FeedUserActionType::kShare);
  histogram_.ExpectBucketCount(
      "ContentSuggestions.Feed.AllFeeds.EngagementType",
      FeedEngagementType::kGoodVisit, 1);
}

TEST_F(MetricsReporterTest, OpenActionSingleWebFeed) {
  reporter_->OpenAction(StreamType(StreamKind::kSingleWebFeed, "A"), 5,
                        OpenActionType::kDefault);

  std::map<FeedEngagementType, int> want({
      {FeedEngagementType::kFeedEngaged, 1},
      {FeedEngagementType::kFeedInteracted, 1},
      {FeedEngagementType::kFeedEngagedSimple, 1},
  });
  EXPECT_EQ(want,
            ReportedEngagementType(StreamType(StreamKind::kSingleWebFeed)));
  EXPECT_EQ(want, ReportedEngagementType(kCombinedStreams));
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.Open"));
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.UserActions",
                                FeedUserActionType::kTappedOnCard, 1);
  histogram_.ExpectUniqueSample("ContentSuggestions.Feed.SingleWebFeed.Opened",
                                5, 1);
  histogram_.ExpectTotalCount("ContentSuggestions.Feed.FollowCount.Engaged2",
                              0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.SingleWebFeed.FollowCount.Engaged2", 1);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.FollowCount.Engaged2", 0);
  histogram_.ExpectTotalCount(
      "ContentSuggestions.Feed.AllFeeds.FollowCount.Engaged2", 1);
}

TEST_F(MetricsReporterTest, SingleWebFeed_ReportsLoadStreamStatus) {
  reporter_->OnLoadStream(StreamType(StreamKind::kSingleWebFeed),
                          NetworkLoadResults(), kContentStats,
                          std::make_unique<LoadLatencyTimes>());
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.LoadStreamStatus.Initial",
      LoadStreamStatus::kLoadedFromNetwork, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.LoadStreamStatus.InitialFromStore",
      LoadStreamStatus::kDataInStoreIsStale, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.LoadedCardCount",
      kContentStats.card_count, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.StreamContentSizeKB",
      kContentStats.total_content_frame_size_bytes / 1024, 1);
  histogram_.ExpectUniqueSample(
      "ContentSuggestions.Feed.SingleWebFeed.SharedStateSizeKB",
      kContentStats.shared_state_size / 1024, 1);
}

}  // namespace feed
