// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_logging_metrics.h"

#include <cmath>
#include <string>
#include <type_traits>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"

namespace feed {

namespace {

//  Instead of using base::UMA_HISTOGRAM_TIMES which has 50 buckets, we use 20
//  buckets here.
#define TASK_CUSTOM_UMA_HISTOGRAM_TIMES(histogram_name, time)         \
  UMA_HISTOGRAM_CUSTOM_TIMES(histogram_name,                          \
                             base::TimeDelta::FromMilliseconds(time), \
                             base::TimeDelta::FromMilliseconds(1),    \
                             base::TimeDelta::FromSeconds(10), 20);

#define REPORT_TASK_HISTOGRAM_TIMES(histogram_base, delay_time, task_time) \
  TASK_CUSTOM_UMA_HISTOGRAM_TIMES(                                         \
      base::StringPrintf("%s.%s", histogram_base.c_str(), "DelayTime"),    \
      delay_time);                                                         \
  TASK_CUSTOM_UMA_HISTOGRAM_TIMES(                                         \
      base::StringPrintf("%s.%s", histogram_base.c_str(), "TaskTime"),     \
      task_time);

#define AGE_CUSTOM_UMA_HISTOGRAM_TIMES(histogram_name, time)  \
  UMA_HISTOGRAM_CUSTOM_TIMES(histogram_name, time,            \
                             base::TimeDelta::FromSeconds(1), \
                             base::TimeDelta::FromDays(7), 100);

// The constant integers(bucket sizes) and strings(UMA names) in this file need
// matching with Zine's in the file
// components/ntp_snippets/content_suggestions_metrics.cc. The purpose to have
// identical bucket sizes and names with Zine is for comparing Feed with Zine
// easily. After Zine is deprecated, we can change the values if we needed.

// Constants used as max sample sizes for histograms.
const int kMaxContentCount = 50;
const int kMaxFailureCount = 10;
const int kMaxSuggestionsTotal = 50;
const int kMaxTokenCount = 10;

// Keep in sync with MAX_SUGGESTIONS_PER_SECTION in NewTabPageUma.java.
const int kMaxSuggestionsForArticle = 20;

const char kHistogramArticlesUsageTimeLocal[] =
    "NewTabPage.ContentSuggestions.UsageTimeLocal";

// Values correspond to
// third_party/feed/src/src/main/java/com/google/android/libraries/feed/host/
// logging/SpinnerType.java, enums.xml and histograms.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SpinnerType {
  KInitialLoad = 1,
  KZeroStateRefresh = 2,
  KMoreButton = 3,
  KSyntheticToken = 4,
  KInfiniteFeed = 5,
  kMaxValue = KInfiniteFeed
};

// Values correspond to
// third_party/feed/src/src/main/java/com/google/android/libraries/feed/host/
// logging/Task.java.
enum class TaskType {
  KUnknown = 0,
  KCleanUpSessionJournals = 1,
  KClearAll = 2,
  KClearAllWithRefresh = 3,
  KClearPersistentStoreTask = 4,
  KCommitTask = 5,
  KCreateAndUpload = 6,
  KDetachSession = 7,
  KDismissLocal = 8,
  KDumpEphemeralActions = 9,
  KExecuteUploadActionRequest = 10,
  KGarbageCollectContent = 11,
  KGetExistingSession = 12,
  KGetNewSession = 13,
  KGetStreamFeaturesFromHead = 14,
  KHandleResponseBytes = 15,
  KHandleSyntheticToken = 16,
  KHandleToken = 17,
  KHandleUploadableActionResponseBytes = 18,
  KInvalidateHead = 19,
  KInvalidateSession = 20,
  KLocalActionGC = 21,
  KNoCardErrorClear = 22,
  KPersistMutation = 23,
  KPopulateNewSession = 24,
  KRequestFailure = 25,
  KRequestManagerTriggerRefresh = 26,
  KSendRequest = 27,
  KSessionManagerTriggerRefresh = 28,
  KSessionMutation = 29,
  KTaskQueueInitialize = 30,
  KUpdateContentTracker = 31,
  KUploadAllActionsForURL = 32,
  kMaxValue = KUploadAllActionsForURL
};

// Values correspond to
// third_party/feed/src/main/proto/search/now/ui/action/feed_action.proto.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ElementType {
  KUnknownElementType = 0,
  KCardLargeImage = 1,
  KCardSmallImage = 2,
  KInterestHeader = 3,
  KTooltip = 4,
  kMaxValue = KTooltip
};

// Each suffix here should correspond to an entry under histogram suffix
// FeedSpinnerType in histograms.xml.
std::string GetSpinnerTypeSuffix(SpinnerType spinner_type) {
  switch (spinner_type) {
    case SpinnerType::KInitialLoad:
      return "InitialLoad";
    case SpinnerType::KZeroStateRefresh:
      return "ZeroStateRefresh";
    case SpinnerType::KMoreButton:
      return "MoreButton";
    case SpinnerType::KSyntheticToken:
      return "SyntheticToken";
    case SpinnerType::KInfiniteFeed:
      return "InfiniteFeed";
  }

  // TODO(https://crbug.com/935602): Handle new values when adding new values on
  // java side.
  NOTREACHED();
  return std::string();
}

// Each suffix here should correspond to an entry under histogram suffix
// FeedElementType in histograms.xml.
std::string GetElementTypeSuffix(ElementType element_type) {
  switch (element_type) {
    case ElementType::KUnknownElementType:
      return "UnknownElementType";
    case ElementType::KCardLargeImage:
      return "CardLargeImage";
    case ElementType::KCardSmallImage:
      return "CardSmallImage";
    case ElementType::KInterestHeader:
      return "InterestHeader";
    case ElementType::KTooltip:
      return "Tooltip";
  }

  NOTREACHED();
  return std::string();
}

// Each suffix here should correspond to an entry under histogram suffix
// FeedTaskType in histograms.xml.
void ReportTaskTime(TaskType task_type, int delay_time_ms, int task_time_ms) {
  switch (task_type) {
    case TaskType::KUnknown:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s", "Unknown"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KCleanUpSessionJournals:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "CleanUpSessionJournals"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KClearAll:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s", "ClearAll"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KClearAllWithRefresh:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "ClearAllWithRefresh"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KClearPersistentStoreTask:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "ClearPersistentStoreTask"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KCommitTask:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s", "CommitTask"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KCreateAndUpload:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "CreateAndUpload"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KDetachSession:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "DetachSession"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KDismissLocal:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s", "DismissLocal"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KDumpEphemeralActions:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "DumpEphemeralActions"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KExecuteUploadActionRequest:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "ExecuteUploadActionRequest"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KGarbageCollectContent:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "GarbageCollectContent"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KGetExistingSession:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "GetExistingSession"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KGetNewSession:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "GetNewSession"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KGetStreamFeaturesFromHead:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "GetStreamFeaturesFromHead"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KHandleResponseBytes:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "HandleResponseBytes"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KHandleSyntheticToken:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "HandleSyntheticToken"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KHandleToken:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s", "HandleToken"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KHandleUploadableActionResponseBytes:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "HandleUploadableActionResponseBytes"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KInvalidateHead:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "InvalidateHead"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KInvalidateSession:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "InvalidateSession"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KLocalActionGC:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "LocalActionGC"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KNoCardErrorClear:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "NoCardErrorClear"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KPersistMutation:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "PersistMutation"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KPopulateNewSession:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "PopulateNewSession"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KRequestFailure:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "RequestFailure"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KRequestManagerTriggerRefresh:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "RequestManagerTriggerRefresh"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KSendRequest:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s", "SendRequest"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KSessionManagerTriggerRefresh:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "SessionManagerTriggerRefresh"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KSessionMutation:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "SessionMutation"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KTaskQueueInitialize:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "TaskQueueInitialize"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KUpdateContentTracker:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "UpdateContentTracker"),
          delay_time_ms, task_time_ms);
      break;
    case TaskType::KUploadAllActionsForURL:
      REPORT_TASK_HISTOGRAM_TIMES(
          base::StringPrintf("ContentSuggestions.Feed.Task.%s",
                             "UploadAllActionsForURL"),
          delay_time_ms, task_time_ms);
      break;
  }
}

// Records ContentSuggestions usage. Therefore the day is sliced into 20min
// buckets. Depending on the current local time the count of the corresponding
// bucket is increased.
void RecordContentSuggestionsUsage(base::Time now) {
  const int kBucketSizeMins = 20;
  const int kNumBuckets = 24 * 60 / kBucketSizeMins;

  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
  int bucket = (now_exploded.hour * 60 + now_exploded.minute) / kBucketSizeMins;

  const char* kWeekdayNames[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
  std::string histogram_name(
      base::StringPrintf("%s.%s", kHistogramArticlesUsageTimeLocal,
                         kWeekdayNames[now_exploded.day_of_week]));
  // Since the |histogram_name| is dynamic, we can't use the regular macro.
  base::UmaHistogramExactLinear(histogram_name, bucket, kNumBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR(kHistogramArticlesUsageTimeLocal, bucket,
                             kNumBuckets);

  base::RecordAction(
      base::UserMetricsAction("NewTabPage_ContentSuggestions_ArticlesUsage"));
}

int ToUMAScore(float score) {
  // Scores are typically reported in a range of (0,1]. As UMA does not support
  // floats, we put them on a discrete scale of [1,10]. We keep the extra bucket
  // 11 for unexpected over-flows as we want to distinguish them from scores
  // close to 1. For instance, the discrete value 1 represents score values
  // within (0.0, 0.1].
  return ceil(score * 10);
}

void RecordSuggestionPageVisited(bool return_to_ntp) {
  if (return_to_ntp) {
    base::RecordAction(
        base::UserMetricsAction("MobileNTP.Snippets.VisitEndBackInNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileNTP.Snippets.VisitEnd"));
}

void RecordUndoableActionUMA(const std::string& histogram_base,
                             int position,
                             bool committed) {
  std::string histogram_name =
      histogram_base + (committed ? ".Commit" : ".Undo");

  // Since the |histogram_name| is dynamic, we can't use the regular macro.
  base::UmaHistogramExactLinear(histogram_name, position, kMaxSuggestionsTotal);

  // Report to |histogram_base| as well, then |histogram_base| will be the sum
  // of "Commit" and "Undo".
  base::UmaHistogramExactLinear(histogram_base, position, kMaxSuggestionsTotal);
}

void CheckURLVisitedDone(int position, bool committed, bool visited) {
  if (visited) {
    RecordUndoableActionUMA("NewTabPage.ContentSuggestions.DismissedVisited",
                            position, committed);
  } else {
    RecordUndoableActionUMA("NewTabPage.ContentSuggestions.DismissedUnvisited",
                            position, committed);
  }
}

void RecordSpinnerTimeUMA(const char* base_name,
                          const base::TimeDelta& time,
                          int spinner_type) {
  SpinnerType type = static_cast<SpinnerType>(spinner_type);
  std::string suffix = GetSpinnerTypeSuffix(type);
  std::string histogram_name(
      base::StringPrintf("%s.%s", base_name, suffix.c_str()));
  base::UmaHistogramTimes(histogram_name, time);
  base::UmaHistogramTimes(base_name, time);
}

void RecordElementPositionUMA(const char* base_name,
                              int position,
                              int element_type) {
  ElementType type = static_cast<ElementType>(element_type);
  std::string suffix = GetElementTypeSuffix(type);
  std::string histogram_name(
      base::StringPrintf("%s.%s", base_name, suffix.c_str()));
  base::UmaHistogramExactLinear(histogram_name, position, kMaxSuggestionsTotal);
  base::UmaHistogramExactLinear(base_name, position, kMaxSuggestionsTotal);
}

void RecordElementTimeUMA(const char* base_name,
                          const base::TimeDelta& time,
                          int element_type) {
  ElementType type = static_cast<ElementType>(element_type);
  std::string suffix = GetElementTypeSuffix(type);
  std::string histogram_name(
      base::StringPrintf("%s.%s", base_name, suffix.c_str()));
  base::UmaHistogramCustomTimes(histogram_name, time,
                                base::TimeDelta::FromSeconds(1),
                                base::TimeDelta::FromDays(7), 100);
  base::UmaHistogramCustomTimes(base_name, time,
                                base::TimeDelta::FromSeconds(1),
                                base::TimeDelta::FromDays(7), 100);
}

}  // namespace

FeedLoggingMetrics::FeedLoggingMetrics(
    HistoryURLCheckCallback history_url_check_callback,
    base::Clock* clock,
    FeedSchedulerHost* scheduler_host)
    : history_url_check_callback_(std::move(history_url_check_callback)),
      clock_(clock),
      scheduler_host_(scheduler_host) {
  DCHECK(scheduler_host_);
}

FeedLoggingMetrics::~FeedLoggingMetrics() = default;

void FeedLoggingMetrics::OnPageShown(const int suggestions_count) {
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible",
      suggestions_count, kMaxSuggestionsTotal);
}

void FeedLoggingMetrics::OnPagePopulated(base::TimeDelta timeToPopulate) {
  UMA_HISTOGRAM_MEDIUM_TIMES("ContentSuggestions.Feed.PagePopulatingTime",
                             timeToPopulate);
}

void FeedLoggingMetrics::OnSuggestionShown(int position,
                                           base::Time publish_date,
                                           float score,
                                           base::Time fetch_date,
                                           bool is_available_offline) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.Shown", position,
                             kMaxSuggestionsTotal);

  base::TimeDelta age = clock_->Now() - publish_date;
  AGE_CUSTOM_UMA_HISTOGRAM_TIMES(
      "NewTabPage.ContentSuggestions.ShownAge.Articles", age);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.ShownScoreNormalized.Articles",
      ToUMAScore(score), 11);

  // Records the time since the fetch time of the displayed snippet.
  base::TimeDelta fetch_age = clock_->Now() - fetch_date;
  AGE_CUSTOM_UMA_HISTOGRAM_TIMES(
      "NewTabPage.ContentSuggestions.TimeSinceSuggestionFetched", fetch_age);

  // When the first of the articles suggestions is shown, then we count this as
  // a single usage of content suggestions.
  if (position == 0) {
    RecordContentSuggestionsUsage(clock_->Now());
  }

  UMA_HISTOGRAM_BOOLEAN("ContentSuggestions.Feed.AvailableOffline.Shown",
                        is_available_offline);
}

void FeedLoggingMetrics::OnSuggestionOpened(int position,
                                            base::Time publish_date,
                                            float score,
                                            bool is_available_offline) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.Opened", position,
                             kMaxSuggestionsTotal);

  base::TimeDelta age = clock_->Now() - publish_date;
  AGE_CUSTOM_UMA_HISTOGRAM_TIMES(
      "NewTabPage.ContentSuggestions.OpenedAge.Articles", age);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.OpenedScoreNormalized.Articles",
      ToUMAScore(score), 11);

  RecordContentSuggestionsUsage(clock_->Now());

  base::RecordAction(base::UserMetricsAction("Suggestions.Content.Opened"));
  UMA_HISTOGRAM_BOOLEAN("ContentSuggestions.Feed.AvailableOffline.Opened",
                        is_available_offline);
}

void FeedLoggingMetrics::OnSuggestionWindowOpened(
    WindowOpenDisposition disposition) {
  // We use WindowOpenDisposition::MAX_VALUE + 1 for |value_max| since MAX_VALUE
  // itself is a valid (and used) enum value.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.OpenDisposition.Articles",
      static_cast<int>(disposition),
      static_cast<int>(WindowOpenDisposition::MAX_VALUE) + 1);

  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    base::RecordAction(base::UserMetricsAction("Suggestions.Card.Tapped"));
  }
}

void FeedLoggingMetrics::OnSuggestionMenuOpened(int position,
                                                base::Time publish_date,
                                                float score) {
  UMA_HISTOGRAM_EXACT_LINEAR("NewTabPage.ContentSuggestions.MenuOpened",
                             position, kMaxSuggestionsTotal);

  base::TimeDelta age = clock_->Now() - publish_date;
  AGE_CUSTOM_UMA_HISTOGRAM_TIMES(
      "NewTabPage.ContentSuggestions.MenuOpenedAge.Articles", age);

  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MenuOpenedScoreNormalized.Articles",
      ToUMAScore(score), 11);
}

void FeedLoggingMetrics::OnSuggestionDismissed(int position,
                                               const GURL& url,
                                               bool committed) {
  history_url_check_callback_.Run(
      url, base::BindOnce(&CheckURLVisitedDone, position, committed));

  base::RecordAction(base::UserMetricsAction("Suggestions.Content.Dismissed"));
}

void FeedLoggingMetrics::OnSuggestionSwiped() {
  base::RecordAction(base::UserMetricsAction("Suggestions.Card.SwipedAway"));
}

void FeedLoggingMetrics::OnSuggestionArticleVisited(base::TimeDelta visit_time,
                                                    bool return_to_ntp) {
  base::UmaHistogramLongTimes(
      "NewTabPage.ContentSuggestions.VisitDuration.Articles", visit_time);
  RecordSuggestionPageVisited(return_to_ntp);
}

void FeedLoggingMetrics::OnSuggestionOfflinePageVisited(
    base::TimeDelta visit_time,
    bool return_to_ntp) {
  base::UmaHistogramLongTimes(
      "NewTabPage.ContentSuggestions.VisitDuration.Downloads", visit_time);
  RecordSuggestionPageVisited(return_to_ntp);
}

void FeedLoggingMetrics::OnMoreButtonShown(int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MoreButtonShown.Articles", position,
      kMaxSuggestionsForArticle + 1);
}

void FeedLoggingMetrics::OnMoreButtonClicked(int position) {
  // Inform the user classifier that a suggestion was consumed
  // (https://crbug.com/992517).
  scheduler_host_->OnSuggestionConsumed();

  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  UMA_HISTOGRAM_EXACT_LINEAR(
      "NewTabPage.ContentSuggestions.MoreButtonClicked.Articles", position,
      kMaxSuggestionsForArticle + 1);
}

void FeedLoggingMetrics::OnNotInterestedInSource(int position, bool committed) {
  RecordUndoableActionUMA(
      "ContentSuggestions.Feed.InterestHeader.NotInterestedInSource", position,
      committed);
}

void FeedLoggingMetrics::OnNotInterestedInTopic(int position, bool committed) {
  RecordUndoableActionUMA(
      "ContentSuggestions.Feed.InterestHeader.NotInterestedInTopic", position,
      committed);
}

void FeedLoggingMetrics::OnSpinnerStarted(int spinner_type) {
  // TODO(https://crbug.com/935602): Handle new values when adding new values on
  // java side.
  SpinnerType type = static_cast<SpinnerType>(spinner_type);
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.FetchPendingSpinner.Shown",
                            type);
}

void FeedLoggingMetrics::OnSpinnerFinished(base::TimeDelta shown_time,
                                           int spinner_type) {
  RecordSpinnerTimeUMA(
      "ContentSuggestions.Feed.FetchPendingSpinner.VisibleDuration", shown_time,
      spinner_type);
}

void FeedLoggingMetrics::OnSpinnerDestroyedWithoutCompleting(
    base::TimeDelta shown_time,
    int spinner_type) {
  RecordSpinnerTimeUMA(
      "ContentSuggestions.Feed.FetchPendingSpinner."
      "VisibleDurationWithoutCompleting",
      shown_time, spinner_type);
}

void FeedLoggingMetrics::OnPietFrameRenderingEvent(
    std::vector<int> piet_error_codes) {
  for (auto error_code : piet_error_codes) {
    base::UmaHistogramSparse(
        "ContentSuggestions.Feed.Piet.FrameRenderingErrorCode", error_code);
  }
}

void FeedLoggingMetrics::OnVisualElementClicked(int element_type,
                                                int position,
                                                base::Time fetch_date) {
  RecordElementPositionUMA("ContentSuggestions.Feed.VisualElement.Clicked",
                           position, element_type);

  RecordElementTimeUMA(
      "ContentSuggestions.Feed.VisualElement.Clicked."
      "TimeSinceElementFetched",
      clock_->Now() - fetch_date, element_type);
}

void FeedLoggingMetrics::OnVisualElementViewed(int element_type,
                                               int position,
                                               base::Time fetch_date) {
  RecordElementPositionUMA("ContentSuggestions.Feed.VisualElement.Viewed",
                           position, element_type);

  RecordElementTimeUMA(
      "ContentSuggestions.Feed.VisualElement.Viewed.TimeSinceElementFetched",
      clock_->Now() - fetch_date, element_type);
}

void FeedLoggingMetrics::OnInternalError(int internal_error) {
  // TODO(https://crbug.com/935602): The max value here is fragile, figure out
  // some way to test the @IntDef size. For now the count needs to be kept in
  // sync with InternalFeedError.java and enums.xml.
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.InternalError",
                            internal_error, 18);
}

void FeedLoggingMetrics::OnTokenCompleted(bool was_synthetic,
                                          int content_count,
                                          int token_count) {
  if (was_synthetic) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.ContentCount2.Synthetic",
        content_count, kMaxContentCount);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.TokenCount.Synthetic",
        token_count, kMaxTokenCount);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.ContentCount2.NotSynthetic",
        content_count, kMaxContentCount);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenCompleted.TokenCount.NotSynthetic",
        token_count, kMaxTokenCount);
  }
}

void FeedLoggingMetrics::OnTokenFailedToComplete(bool was_synthetic,
                                                 int failure_count) {
  if (was_synthetic) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenFailedToCompleted.Synthetic",
        failure_count, kMaxFailureCount);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "ContentSuggestions.Feed.TokenFailedToCompleted.NotSynthetic",
        failure_count, kMaxFailureCount);
  }
}

void FeedLoggingMetrics::OnServerRequest(int request_reason) {
  // TODO(https://crbug.com/935602): The max value here is fragile, figure out
  // some way to test the @IntDef size.
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.ServerRequest.Reason",
                            request_reason, 8);
}

void FeedLoggingMetrics::OnZeroStateShown(int zero_state_show_reason) {
  // TODO(https://crbug.com/935602): The max value here is fragile, figure out
  // some way to test the @IntDef size.
  UMA_HISTOGRAM_ENUMERATION("ContentSuggestions.Feed.ZeroStateShown.Reason",
                            zero_state_show_reason, 3);
}

void FeedLoggingMetrics::OnZeroStateRefreshCompleted(int new_content_count,
                                                     int new_token_count) {
  UMA_HISTOGRAM_EXACT_LINEAR(
      "ContentSuggestions.Feed.ZeroStateRefreshCompleted.ContentCount",
      new_content_count, kMaxContentCount);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "ContentSuggestions.Feed.ZeroStateRefreshCompleted.TokenCount",
      new_token_count, kMaxTokenCount);
}

void FeedLoggingMetrics::OnTaskFinished(int task_type,
                                        int delay_time_ms,
                                        int task_time_ms) {
  TaskType type = static_cast<TaskType>(task_type);
  ReportTaskTime(type, delay_time_ms, task_time_ms);
}

void FeedLoggingMetrics::ReportScrolledAfterOpen() {
  base::RecordAction(base::UserMetricsAction("Suggestions.ScrolledAfterOpen"));
}

}  // namespace feed
