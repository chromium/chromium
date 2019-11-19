// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_
#define COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "url/gurl.h"

namespace base {
class Clock;
class Time;
class TimeDelta;
}  // namespace base

enum class WindowOpenDisposition;

namespace feed {

// FeedLoggingMetrics is a central place to report all the UMA metrics for Feed.
class FeedLoggingMetrics {
 public:
  // Return whether the URL is visited when calling checking URL visited.
  using CheckURLVisitCallback = base::OnceCallback<void(bool)>;

  // Calling this callback when need to check whether url is visited, and will
  // tell CheckURLVisitCallback the result.
  using HistoryURLCheckCallback =
      base::RepeatingCallback<void(const GURL&, CheckURLVisitCallback)>;

  explicit FeedLoggingMetrics(HistoryURLCheckCallback callback,
                              base::Clock* clock,
                              FeedSchedulerHost* scheduler_host);
  ~FeedLoggingMetrics();

  // |suggestions_count| contains how many cards show to users. It does not
  // depend on whether the user actually saw the cards.
  void OnPageShown(const int suggestions_count);

  // The amount of time for the Feed to populate articles. This does not include
  // time to render but time to populate data in the UI.
  void OnPagePopulated(base::TimeDelta timeToPopulate);

  // Should only be called once per NTP for each suggestion.
  void OnSuggestionShown(int position,
                         base::Time publish_date,
                         float score,
                         base::Time fetch_date,
                         bool is_available_offline);

  void OnSuggestionOpened(int position,
                          base::Time publish_date,
                          float score,
                          bool is_available_offline);

  void OnSuggestionWindowOpened(WindowOpenDisposition disposition);

  void OnSuggestionMenuOpened(int position,
                              base::Time publish_date,
                              float score);

  void OnSuggestionDismissed(int position, const GURL& url, bool committed);

  void OnSuggestionSwiped();

  void OnSuggestionArticleVisited(base::TimeDelta visit_time,
                                  bool return_to_ntp);

  void OnSuggestionOfflinePageVisited(base::TimeDelta visit_time,
                                      bool return_to_ntp);

  // Should only be called once per NTP for each "more" button.
  void OnMoreButtonShown(int position);

  void OnMoreButtonClicked(int position);

  void OnNotInterestedInSource(int position, bool committed);

  void OnNotInterestedInTopic(int position, bool committed);

  void OnSpinnerStarted(int spinner_type);

  void OnSpinnerFinished(base::TimeDelta shown_time, int spinner_type);

  void OnSpinnerDestroyedWithoutCompleting(base::TimeDelta shown_time,
                                           int spinner_type);

  void OnPietFrameRenderingEvent(std::vector<int> piet_error_codes);

  void OnVisualElementClicked(int element_type,
                              int position,
                              base::Time fetch_date);

  void OnVisualElementViewed(int element_type,
                             int position,
                             base::Time fetch_date);

  void OnInternalError(int internal_error);

  void OnTokenCompleted(bool was_synthetic, int content_count, int token_count);

  void OnTokenFailedToComplete(bool was_synthetic, int failure_count);

  void OnServerRequest(int request_reason);

  void OnZeroStateShown(int zero_state_show_reason);

  void OnZeroStateRefreshCompleted(int new_content_count, int new_token_count);

  void OnTaskFinished(int task_type, int delay_time_ms, int task_time_ms);

  void ReportScrolledAfterOpen();

 private:
  const HistoryURLCheckCallback history_url_check_callback_;

  // Used to access current time, injected for testing.
  base::Clock* clock_;

  FeedSchedulerHost* scheduler_host_;

  base::WeakPtrFactory<FeedLoggingMetrics> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedLoggingMetrics);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_LOGGING_METRICS_H_
