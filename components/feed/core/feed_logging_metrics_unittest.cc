// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_logging_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"

namespace feed {
namespace {

GURL VISITED_URL("http://visited_url.com");

using testing::ElementsAre;
using testing::IsEmpty;

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
  if (url == VISITED_URL) {
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace

class FeedLoggingMetricsTest : public testing::Test {
 public:
  FeedLoggingMetricsTest() {
    feed_logging_metrics_ = std::make_unique<FeedLoggingMetrics>(
        base::BindRepeating(&CheckURLVisit));
  }

  FeedLoggingMetrics* feed_logging_metrics() {
    return feed_logging_metrics_.get();
  }

 private:
  std::unique_ptr<FeedLoggingMetrics> feed_logging_metrics_;

  DISALLOW_COPY_AND_ASSIGN(FeedLoggingMetricsTest);
};

TEST_F(FeedLoggingMetricsTest, ShouldLogOnSuggestionsShown) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/1, base::Time::Now(),
      /*score=*/0.01f, base::Time::Now() - base::TimeDelta::FromHours(2));
  // Test corner cases for score.
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/2, base::Time::Now(),
      /*score=*/0.0f, base::Time::Now() - base::TimeDelta::FromHours(2));
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/3, base::Time::Now(),
      /*score=*/1.0f, base::Time::Now() - base::TimeDelta::FromHours(2));
  feed_logging_metrics()->OnSuggestionShown(
      /*position=*/4, base::Time::Now(),
      /*score=*/8.0f, base::Time::Now() - base::TimeDelta::FromHours(2));

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
      /*position=*/11, base::Time::Now(),
      /*score=*/1.0f);
  feed_logging_metrics()->OnSuggestionOpened(
      /*position=*/13, base::Time::Now(),
      /*score=*/1.0f);
  feed_logging_metrics()->OnSuggestionOpened(
      /*position=*/15, base::Time::Now(),
      /*score=*/1.0f);
  feed_logging_metrics()->OnSuggestionOpened(
      /*position=*/23, base::Time::Now(),
      /*score=*/1.0f);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("NewTabPage.ContentSuggestions.Opened"),
      ElementsAre(base::Bucket(/*min=*/11, /*count=*/1),
                  base::Bucket(/*min=*/13, /*count=*/1),
                  base::Bucket(/*min=*/15, /*count=*/1),
                  base::Bucket(/*min=*/23, /*count=*/1)));
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

TEST_F(FeedLoggingMetricsTest, ShouldLogOnSuggestionDismissedIfVisited) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionDismissed(/*position=*/10, VISITED_URL);
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.DismissedVisited"),
              ElementsAre(base::Bucket(/*min=*/10, /*count=*/1)));
}

TEST_F(FeedLoggingMetricsTest, ShouldLogOnSuggestionDismissedIfNotVisited) {
  base::HistogramTester histogram_tester;
  feed_logging_metrics()->OnSuggestionDismissed(/*position=*/10,
                                                GURL("http://non_visited.com"));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "NewTabPage.ContentSuggestions.DismissedVisited"),
              IsEmpty());
}

}  // namespace feed
