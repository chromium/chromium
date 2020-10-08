// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/common/pref_names.h"

#include <string>

#include "components/feed/core/common/user_classifier.h"
#include "components/prefs/pref_registry_simple.h"

namespace feed {

namespace prefs {

const char kBackgroundRefreshPeriod[] = "feed.background_refresh_period";

const char kLastFetchAttemptTime[] = "feed.last_fetch_attempt";

const char kThrottlerRequestCount[] = "feed.refresh_throttler.count";
const char kThrottlerRequestsDay[] = "feed.refresh_throttler.day";

const char kUserClassifierAverageSuggestionsViwedPerHour[] =
    "feed.user_classifier.average_suggestions_veiwed_per_hour";
const char kUserClassifierAverageSuggestionsUsedPerHour[] =
    "feed.user_classifier.average_suggestions_used_per_hour";

const char kUserClassifierLastTimeToViewSuggestions[] =
    "feed.user_classifier.last_time_to_view_suggestions";
const char kUserClassifierLastTimeToUseSuggestions[] =
    "feed.user_classifier.last_time_to_use_suggestions";

const char kHostOverrideHost[] = "feed.host_override.host";
const char kHostOverrideBlessNonce[] = "feed.host_override.bless_nonce";

const char kHasReachedClickAndViewActionsUploadConditions[] =
    "feed.clicks_and_views_upload_conditions_reached";
const char kLastFetchHadNoticeCard[] = "feed.last_fetch_had_notice_card";

const char kThrottlerRequestCountListPrefName[] =
    "feedv2.request_throttler.request_counts";
const char kThrottlerLastRequestTime[] =
    "feedv2.request_throttler.last_request_time";
const char kDebugStreamData[] = "feedv2.debug_stream_data";
const char kRequestSchedule[] = "feedv2.request_schedule";
const char kMetricsData[] = "feedv2.metrics_data";
const char kClientInstanceId[] = "feedv2.client_instance_id";
const char kActionsEndpointOverride[] = "feedv2.actions_endpoint_override";

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(feed::prefs::kHostOverrideHost, "");
  registry->RegisterStringPref(feed::prefs::kHostOverrideBlessNonce, "");
  registry->RegisterIntegerPref(feed::prefs::kThrottlerRequestCount, 0);
  registry->RegisterIntegerPref(feed::prefs::kThrottlerRequestsDay, 0);
  registry->RegisterTimePref(prefs::kLastFetchAttemptTime, base::Time());
  registry->RegisterTimeDeltaPref(prefs::kBackgroundRefreshPeriod,
                                  base::TimeDelta());
  registry->RegisterListPref(feed::prefs::kThrottlerRequestCountListPrefName);
  registry->RegisterTimePref(feed::prefs::kThrottlerLastRequestTime,
                             base::Time());
  registry->RegisterStringPref(feed::prefs::kDebugStreamData, std::string());
  registry->RegisterDictionaryPref(feed::prefs::kRequestSchedule);
  registry->RegisterDictionaryPref(feed::prefs::kMetricsData);
  registry->RegisterStringPref(feed::prefs::kClientInstanceId, "");
  registry->RegisterStringPref(feed::prefs::kActionsEndpointOverride, "");
  registry->RegisterBooleanPref(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions, false);
  registry->RegisterBooleanPref(feed::prefs::kLastFetchHadNoticeCard, true);
  UserClassifier::RegisterProfilePrefs(registry);
}

}  // namespace feed
