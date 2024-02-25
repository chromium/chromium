// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/request_throttler.h"

#include <vector>

#include "base/logging.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/prefs.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace {
// Returns the maximum number of requests per day for this request type.
// -1 indicates there is no limit.
int GetMaxRequestsPerDay(NetworkRequestType request_type) {
  const Config& config = GetFeedConfig();
  switch (request_type) {
    case NetworkRequestType::kFeedQuery:
    case NetworkRequestType::kWebFeedListContents:
    case NetworkRequestType::kQueryInteractiveFeed:
    case NetworkRequestType::kQueryBackgroundFeed:
      return config.max_feed_query_requests_per_day;
    case NetworkRequestType::kUploadActions:
      return config.max_action_upload_requests_per_day;
    case NetworkRequestType::kNextPage:
    case NetworkRequestType::kQueryNextPage:
      return config.max_next_page_requests_per_day;
    case NetworkRequestType::kListWebFeeds:
      return config.max_list_web_feeds_requests_per_day;
    case NetworkRequestType::kListRecommendedWebFeeds:
      return config.max_list_recommended_web_feeds_requests_per_day;
    case NetworkRequestType::kUnfollowWebFeed:
    case NetworkRequestType::kFollowWebFeed:
    case NetworkRequestType::kSingleWebFeedListContents:
    case NetworkRequestType::kQueryWebFeed:
    case NetworkRequestType::kSupervisedFeed:
      return -1;
  }
}

int DaysSinceOrigin(const base::Time& time_value) {
  // |LocalMidnight()| DCHECKs on some platforms if |time_value| is too small
  // (like zero). So if time is before the unix epoch, return 0.
  return time_value < base::Time::UnixEpoch()
             ? 0
             : time_value.LocalMidnight().since_origin().InDays();
}

}  // namespace

RequestThrottler::RequestThrottler(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

bool RequestThrottler::RequestQuota(NetworkRequestType request_type) {
  ResetCountersIfDayChanged();

  const int max_requests_per_day = GetMaxRequestsPerDay(request_type);
  if (max_requests_per_day == -1)
    return true;

  // Fetch request counts from prefs. There's an entry for each request type.
  // We may need to resize the list.
  std::vector<int> request_counts =
      feed::prefs::GetThrottlerRequestCounts(*pref_service_);
  const size_t request_count_index = static_cast<size_t>(request_type);
  if (request_counts.size() <= request_count_index)
    request_counts.resize(request_count_index + 1);

  int& requests_already_made = request_counts[request_count_index];
  if (requests_already_made >= max_requests_per_day)
    return false;
  requests_already_made++;
  feed::prefs::SetThrottlerRequestCounts(request_counts, *pref_service_);
  return true;
}

void RequestThrottler::ResetCountersIfDayChanged() {
  // Grant new quota on local midnight to spread out when clients that start
  // making un-throttled requests to server.
  const base::Time now = base::Time::Now();
  const bool day_changed =
      DaysSinceOrigin(feed::prefs::GetLastRequestTime(*pref_service_)) !=
      DaysSinceOrigin(now);
  feed::prefs::SetLastRequestTime(now, *pref_service_);

  if (day_changed)
    feed::prefs::SetThrottlerRequestCounts({}, *pref_service_);
}

}  // namespace feed
