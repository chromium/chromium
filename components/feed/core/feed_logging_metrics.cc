// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_logging_metrics.h"

#include <cmath>
#include <string>
#include <type_traits>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"

namespace feed {

namespace {

// The constant integers(bucket sizes) and strings(UMA names) in this file need
// matching with Zine's in the file
// components/ntp_snippets/content_suggestions_metrics.cc. The purpose to have
// identical bucket sizes and names with Zine is for comparing Feed with Zine
// easily. After Zine is deprecated, we can change the values if we needed.

const int kMaxSuggestionsTotal = 50;

// Keep in sync with MAX_SUGGESTIONS_PER_SECTION in NewTabPageUma.java.
const int kMaxSuggestionsForArticle = 20;

const char kHistogramArticlesUsageTimeLocal[] =
    "NewTabPage.ContentSuggestions.UsageTimeLocal";

// Records ContentSuggestions usage. Therefore the day is sliced into 20min
// buckets. Depending on the current local time the count of the corresponding
// bucket is increased.
void RecordContentSuggestionsUsage() {
  const int kBucketSizeMins = 20;
  const int kNumBuckets = 24 * 60 / kBucketSizeMins;

  base::Time::Exploded now_exploded;
  base::Time::Now().LocalExplode(&now_exploded);
  int bucket = (now_exploded.hour * 60 + now_exploded.minute) / kBucketSizeMins;

  const char* kWeekdayNames[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
  std::string histogram_name(
      base::StringPrintf("%s.%s", kHistogramArticlesUsageTimeLocal,
                         kWeekdayNames[now_exploded.day_of_week]));
  base::UmaHistogramExactLinear(histogram_name, bucket, kNumBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramArticlesUsageTimeLocal, bucket,
                             kNumBuckets);
}

int ToUMAScore(float score) {
  // Scores are typically reported in a range of (0,1]. As UMA does not support
  // floats, we put them on a discrete scale of [1,10]. We keep the extra bucket
  // 11 for unexpected over-flows as we want to distinguish them from scores
  // close to 1. For instance, the discrete value 1 represents score values
  // within (0.0, 0.1].
  return ceil(score * 10);
}

}  // namespace

FeedLoggingMetrics::FeedLoggingMetrics(
    HistoryURLCheckCallback history_url_check_callback)
    : history_url_check_callback_(std::move(history_url_check_callback)),
      weak_ptr_factory_(this) {}

FeedLoggingMetrics::~FeedLoggingMetrics() = default;

void FeedLoggingMetrics::OnPageShown(const int suggestions_count) {
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible",
      suggestions_count, kMaxSuggestionsTotal);
}

void FeedLoggingMetrics::OnSuggestionShown(int position,
                                           base::Time publish_date,
                                           float score,
                                           base::Time fetch_date) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.Shown", position,
                             kMaxSuggestionsTotal);

  base::TimeDelta age = base::Time::Now() - publish_date;
  UMA_HISTOGRAM_CUSTOM_TIMES("NewTabPage.ContentSuggestions.ShownAge.Articles",
                             age, base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromDays(7), 100);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.ShownScoreNormalized.Articles",
      ToUMAScore(score), 11);

  // Records the time since the fetch time of the displayed snippet.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "NewTabPage.ContentSuggestions.TimeSinceSuggestionFetched",
      base::Time::Now() - fetch_date, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(7),
      /*bucket_count=*/100);

  // When the first of the articles suggestions is shown, then we count this as
  // a single usage of content suggestions.
  if (position == 0) {
    RecordContentSuggestionsUsage();
  }
}

void FeedLoggingMetrics::OnSuggestionOpened(int position,
                                            base::Time publish_date,
                                            float score) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.Opened", position,
                             kMaxSuggestionsTotal);

  base::TimeDelta age = base::Time::Now() - publish_date;
  UMA_HISTOGRAM_CUSTOM_TIMES("NewTabPage.ContentSuggestions.OpenedAge.Articles",
                             age, base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromDays(7), 100);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.OpenedScoreNormalized.Articles",
      ToUMAScore(score), 11);

  RecordContentSuggestionsUsage();
}

void FeedLoggingMetrics::OnSuggestionWindowOpened(
    WindowOpenDisposition disposition) {
  // We use WindowOpenDisposition::MAX_VALUE + 1 for |value_max| since MAX_VALUE
  // itself is a valid (and used) enum value.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.OpenDisposition.Articles",
      static_cast<int>(disposition),
      static_cast<int>(WindowOpenDisposition::MAX_VALUE) + 1);
}

void FeedLoggingMetrics::OnSuggestionMenuOpened(int position,
                                                base::Time publish_date,
                                                float score) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.MenuOpened",
                             position, kMaxSuggestionsTotal);

  base::TimeDelta age = base::Time::Now() - publish_date;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "NewTabPage.ContentSuggestions.MenuOpenedAge.Articles", age,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(7), 100);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MenuOpenedScoreNormalized.Articles",
      ToUMAScore(score), 11);
}

void FeedLoggingMetrics::OnSuggestionDismissed(int position, const GURL& url) {
  history_url_check_callback_.Run(
      url, base::BindOnce(&FeedLoggingMetrics::CheckURLVisitedDone,
                          weak_ptr_factory_.GetWeakPtr(), position));
}

void FeedLoggingMetrics::OnSuggestionArticleVisited(
    base::TimeDelta visit_time) {
  base::UmaHistogramLongTimes(
      "NewTabPage.ContentSuggestions.VisitDuration.Articles", visit_time);
}

void FeedLoggingMetrics::OnSuggestionOfflinePageVisited(
    base::TimeDelta visit_time) {
  base::UmaHistogramLongTimes(
      "NewTabPage.ContentSuggestions.VisitDuration.Downloads", visit_time);
}

void FeedLoggingMetrics::OnMoreButtonShown(int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MoreButtonShown.Articles", position,
      kMaxSuggestionsForArticle + 1);
}

void FeedLoggingMetrics::OnMoreButtonClicked(int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MoreButtonClicked.Articles", position,
      kMaxSuggestionsForArticle + 1);
}

void FeedLoggingMetrics::CheckURLVisitedDone(int position, bool visited) {
  if (visited) {
    UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.DismissedVisited",
                               position, kMaxSuggestionsTotal);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "NewTabPage.ContentSuggestions.DismissedUnvisited", position,
        kMaxSuggestionsTotal);
  }
}

}  // namespace feed
