// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_logging_metrics.h"

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/user_classifier.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

namespace feed {
namespace {

GURL kVisitedUrl("http://visited_url.com/");

// Fixed "now" to make tests more deterministic.
char kNowString[] = "2018-06-11 15:41";

// This needs to keep in sync with ActionType in third_party/feed/src/src/main/
// java/com/google/android/libraries/feed/host/logging/ActionType.java.
enum FeedActionType {
  UNKNOWN = -1,
  OPEN_URL = 1,
  OPEN_URL_INCOGNITO = 2,
  OPEN_URL_NEW_WINDOW = 3,
  OPEN_URL_NEW_TAB = 4,
  DOWNLOAD = 5,
};

void CheckURLVisit(const GURL& url,
                   FeedLoggingMetrics::CheckURLVisitCallback callback) {
  if (url == kVisitedUrl) {
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace

class FeedLoggingMetricsTest : public testing::Test {
 public:
  FeedLoggingMetricsTest() {
    base::Time now;
    EXPECT_TRUE(base::Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);

    feed::RegisterProfilePrefs(prefs_.registry());
    scheduler_host_ =
        std::make_unique<FeedSchedulerHost>(&prefs_, &prefs_, &test_clock_);

    feed_logging_metrics_ = std::make_unique<FeedLoggingMetrics>(
        base::BindRepeating(&CheckURLVisit), &test_clock_,
        scheduler_host_.get());
  }

  FeedLoggingMetrics* feed_logging_metrics() {
    return feed_logging_metrics_.get();
  }
  base::SimpleTestClock* test_clock() { return &test_clock_; }

 private:
  base::SimpleTestClock test_clock_;

  TestingPrefServiceSimple prefs_;

  std::unique_ptr<FeedSchedulerHost> scheduler_host_;

  std::unique_ptr<FeedLoggingMetrics> feed_logging_metrics_;

  DISALLOW_COPY_AND_ASSIGN(FeedLoggingMetricsTest);
};

TEST_F(FeedLoggingMetricsTest, ShouldLogOnSuggestionsShown) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/1, test_clock()->Now(),
      /*score=*/0.01f, test_clock()->Now() - base::TimeDelta::FromHours(2),
      /*is_available_offline=*/false);
  // Test corner cases for score.
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/2, test_clock()->Now(),
      /*score=*/0.0f, test_clock()->Now() - base::TimeDelta::FromHours(2),
      /*is_available_offline=*/true);
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/3, test_clock()->Now(),
      /*score=*/1.0f, test_clock()->Now() - base::TimeDelta::FromHours(2),
      /*is_available_offline=*/true);
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/4, test_clock()->Now(),
      /*score=*/8.0f, test_clock()->Now() - base::TimeDelta::FromHours(2),
      /*is_available_offline=*/true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.ContentSuggestions.Shown"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/2, /*count=*/1),
                  base::Bucket(/*min=*/3, /*count=*/1),
                  base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "NewTabPage.ContentSuggestions.ShownScoreNormalized.Articles"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                  base::Bucket(/*min=*/1, /*count=*/1),
                  base::Bucket(/*min=*/10, /*count=*/1),
                  base::Bucket(/*min=*/11, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "ContentSuggestions.Feed.AvailableOffline.Shown"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/3)));
}

TEST_F(FeedLoggingMetricsTest, ShouldLogOnPageShown) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnPageShown(/*content_count=*/10);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest, ShouldLogOnSuggestionOpened) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionOpened(
      /*position=*/11, test_clock()->Now(),
      /*score=*/1.0f, /*is_available_offline=*/false);
  feed_logging_metrics()->OnSuggestionOpened(
      /*position=*/13, test_clock()->Now(),
      /*score=*/1.0f, /*is_available_offline=*/false);
  feed_logging_metrics()->OnSuggestionOpened(
      /*position=*/15, test_clock()->Now(),
      /*score=*/1.0f, /*is_available_offline=*/false);
  feed_logging_metrics()->OnSuggestionOpened(
      /*position=*/23, test_clock()->Now(),
      /*score=*/1.0f, /*is_available_offline=*/true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.ContentSuggestions.Opened"),
      ElementsAre(base::Bucket(/*min=*/11, /*count=*/1),
                  base::Bucket(/*min=*/13, /*count=*/1),
                  base::Bucket(/*min=*/15, /*count=*/1),
                  base::Bucket(/*min=*/23, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "ContentSuggestions.Feed.AvailableOffline.Opened"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/3),
                          base::Bucket(/*min=*/1, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest, ShouldLogOnSuggestionWindowOpened) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionWindowOpened(
      WindowOpenDisposition::CURRENT_TAB);
  feed_logging_metrics()->OnSuggestionWindowOpened(
      WindowOpenDisposition::CURRENT_TAB);
  feed_logging_metrics()->OnSuggestionWindowOpened(
      WindowOpenDisposition::CURRENT_TAB);
  feed_logging_metrics()->OnSuggestionWindowOpened(
      WindowOpenDisposition::CURRENT_TAB);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.OpenDisposition.Articles"),
              ElementsAre(base::Bucket(
                  /*WindowOpenDisposition::CURRENT_TAB=*/static_cast<int>(
                      WindowOpenDisposition::CURRENT_TAB),
                  /*count=*/4)));
}

TEST_F(FeedLoggingMetricsTest, ShouldLogOnSuggestionDismissedCommitIfVisited) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionDismissed(/*position=*/10, kVisitedUrl,
                                                true);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.DismissedVisited.Commit"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest,
       ShouldLogOnSuggestionDismissedCommitIfNotVisited) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionDismissed(
      /*position=*/10, GURL("http://non_visited.com"), true);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.DismissedUnvisited.Commit"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest,
       ShouldLogOnSuggestionDismissedUndoIfUndoDismissAndVisited) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionDismissed(/*position=*/10, kVisitedUrl,
                                                false);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.DismissedVisited.Undo"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest,
       ShouldLogOnSuggestionDismissedUndoIfUndoDismissAndNotVisited) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionDismissed(
      /*position=*/10, GURL("http://non_visited.com"), false);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.DismissedUnvisited.Undo"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest, ShouldReportOnPietFrameRenderingEvent) {
  base::HistogramTester histogram_tester;
  std::vector<int> error_codes({0, 1, 6, 7});
  feed_logging_metrics()->OnPietFrameRenderingEvent(error_codes);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "ContentSuggestions.Feed.Piet.FrameRenderingErrorCode"),
              ElementsAre(base::Bucket(/*min=*/0, /*count=*/1),
                          base::Bucket(/*min=*/1, /*count=*/1),
                          base::Bucket(/*min=*/6, /*count=*/1),
                          base::Bucket(/*min=*/7, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest, ShouldLogOnTaskFinished) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnTaskFinished(/*KExecuteUploadActionRequest=*/10, 8,
                                         8);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "ContentSuggestions.Feed.Task.ExecuteUploadActionRequest.DelayTime"),
      ElementsAre(base::Bucket(/*min=*/8, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "ContentSuggestions.Feed.Task.ExecuteUploadActionRequest.TaskTime"),
      ElementsAre(base::Bucket(/*min=*/8, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest, ShouldLogOnMoreButtonClicked) {
  base::HistogramTester histogram_tester;

  feed_logging_metrics()->OnMoreButtonClicked(1);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.MoreButtonClicked.Articles"),
              ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  // User classifier should have been informed of a suggestion being consumed.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.UserClassifier.AverageHoursToUseSuggestions"),
              SizeIs(1));
}

}  // namespace feed
