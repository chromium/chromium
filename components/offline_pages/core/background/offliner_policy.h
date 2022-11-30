// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_H_

namespace offline_pages {

// The max number of started tries is to guard against pages that make the
// background loader crash. It should be greater than or equal to the max
// number of completed tries.
constexpr int kMaxStartedTries = 5;
// The number of max completed tries is based on Gin2G-poor testing showing that
// we often need about 4 tries with a 2 minute window, or 3 retries with a 3
// minute window. Also, we count one try now for foreground/disabled requests.
constexpr int kMaxCompletedTries = 3;
// By the time we get to a week, the user has forgotten asking for a page.
constexpr int kRequestExpirationTimeInSeconds = 60 * 60 * 24 * 7;

// Scheduled background processing time limits.
constexpr int kDozeModeBackgroundServiceWindowSeconds = 60 * 3;
constexpr int kDefaultBackgroundProcessingTimeBudgetSeconds =
    kDozeModeBackgroundServiceWindowSeconds - 10;
constexpr int kSinglePageTimeLimitWhenBackgroundScheduledSeconds =
    kDozeModeBackgroundServiceWindowSeconds - 10;

// Immediate processing time limits.  Note: experiments on GIN-2g-poor show many
// page requests took 3 or 4 attempts in background scheduled mode with timeout
// of 2 minutes. So for immediate processing mode, give page requests just under
// 5 minutes, which was equal to the timeout limit in prerender. Then budget up
// to 3 of those requests in processing window.
// TODO(petewil): Consider if we want to up the immediate window to 8 minutes
// now that we are always using the background loader.
constexpr int kSinglePageTimeLimitForImmediateLoadSeconds = 60 * 4 + 50;
constexpr int kImmediateLoadProcessingTimeBudgetSeconds =
    kSinglePageTimeLimitForImmediateLoadSeconds * 5;

// Policy for the Background Offlining system.  Some policy will belong to the
// RequestCoordinator, some to the RequestQueue, and some to the Offliner.
class OfflinerPolicy {
 public:
  OfflinerPolicy()
      : prefer_untried_requests_(true),
        prefer_earlier_requests_(true),
        retry_count_is_more_important_than_recency_(true),
        max_started_tries_(kMaxStartedTries),
        max_completed_tries_(kMaxCompletedTries),
        background_scheduled_processing_time_budget_(
            kDefaultBackgroundProcessingTimeBudgetSeconds) {}

  // Constructor for unit tests.
  OfflinerPolicy(bool prefer_untried,
                 bool prefer_earlier,
                 bool prefer_retry_count,
                 int max_started_tries,
                 int max_completed_tries,
                 int background_processing_time_budget)
      : prefer_untried_requests_(prefer_untried),
        prefer_earlier_requests_(prefer_earlier),
        retry_count_is_more_important_than_recency_(prefer_retry_count),
        max_started_tries_(max_started_tries),
        max_completed_tries_(max_completed_tries),
        background_scheduled_processing_time_budget_(
            background_processing_time_budget) {}

  // TODO(petewil): Numbers here are chosen arbitrarily, do the proper studies
  // to get good policy numbers. Eventually this should get data from a finch
  // experiment. crbug.com/705112.

  // Returns true if we should prefer retrying lesser tried requests.
  bool ShouldPreferUntriedRequests() const { return prefer_untried_requests_; }

  // Returns true if we should prefer older requests of equal number of tries.
  bool ShouldPreferEarlierRequests() const { return prefer_earlier_requests_; }

  // Returns true if retry count is considered more important than recency in
  // picking which request to try next.
  bool RetryCountIsMoreImportantThanRecency() const {
    return retry_count_is_more_important_than_recency_;
  }

  // The max number of times we will start a request.  Not all started attempts
  // will complete.  This may be caused by background loader issues or chromium
  // being swapped out of memory.
  int GetMaxStartedTries() const { return max_started_tries_; }

  // The max number of times we will retry a request when the attempt
  // completed, but failed.
  int GetMaxCompletedTries() const { return max_completed_tries_; }

  bool PowerRequired(bool user_requested) const { return (!user_requested); }

  bool UnmeteredNetworkRequired(bool user_requested) const {
    return !(user_requested);
  }

  int BatteryPercentageRequired(bool user_requested) const {
    if (user_requested)
      return 0;
    // This is so low because we require the device to be plugged in and
    // charging.  If we decide to allow non-user requested pages when not
    // plugged in, we should raise this somewhat higher.
    return 25;
  }

  // How many seconds to keep trying new pages for, before we give up, and
  // return to the scheduler.
  // TODO(dougarnett): Consider parameterizing these time limit/budget
  // calls with processing mode.
  int GetProcessingTimeBudgetWhenBackgroundScheduledInSeconds() const {
    return background_scheduled_processing_time_budget_;
  }

  // How many seconds to keep trying new pages for, before we give up, when
  // processing started immediately (without scheduler).
  int GetProcessingTimeBudgetForImmediateLoadInSeconds() const {
    return kImmediateLoadProcessingTimeBudgetSeconds;
  }

  // How long do we allow a page to load before giving up on it when
  // background loading was scheduled.
  int GetSinglePageTimeLimitWhenBackgroundScheduledInSeconds() const {
    return kSinglePageTimeLimitWhenBackgroundScheduledSeconds;
  }

  // How long do we allow a page to load before giving up on it when
  // immediately background loading.
  int GetSinglePageTimeLimitForImmediateLoadInSeconds() const {
    return kSinglePageTimeLimitForImmediateLoadSeconds;
  }

  // How long we allow requests to remain in the system before giving up.
  int GetRequestExpirationTimeInSeconds() const {
    return kRequestExpirationTimeInSeconds;
  }

 private:
  bool prefer_untried_requests_;
  bool prefer_earlier_requests_;
  bool retry_count_is_more_important_than_recency_;
  int max_started_tries_;
  int max_completed_tries_;
  int background_scheduled_processing_time_budget_;
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_H_
