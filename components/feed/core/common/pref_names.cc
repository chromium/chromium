// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/common/pref_names.h"

#include <string>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace feed {

namespace prefs {

const char kLastFetchAttemptTime[] = "feed.last_fetch_attempt";

const char kHostOverrideHost[] = "feed.host_override.host";
const char kHostOverrideBlessNonce[] = "feed.host_override.bless_nonce";

const char kHasReachedClickAndViewActionsUploadConditions[] =
    "feed.clicks_and_views_upload_conditions_reached";
const char kLastFetchHadNoticeCard[] = "feed.last_fetch_had_notice_card";
#if defined(OS_IOS)
const char kLastFetchHadLoggingEnabled[] =
    "feed.last_fetch_had_logging_enabled";
#endif  // defined(OS_IOS)
const char kNoticeCardViewsCount[] = "feed.notice_card_views_count";
const char kNoticeCardClicksCount[] = "feed.notice_card_clicks_count";

const char kThrottlerRequestCountListPrefName[] =
    "feedv2.request_throttler.request_counts";
const char kThrottlerLastRequestTime[] =
    "feedv2.request_throttler.last_request_time";
const char kDebugStreamData[] = "feedv2.debug_stream_data";
const char kRequestSchedule[] = "feedv2.request_schedule";
const char kWebFeedsRequestSchedule[] = "webfeed.request_schedule";
const char kMetricsData[] = "feedv2.metrics_data";
const char kClientInstanceId[] = "feedv2.client_instance_id";
// This pref applies to all discover APIs despite the string.
const char kDiscoverAPIEndpointOverride[] = "feedv2.actions_endpoint_override";
const char kExperiments[] = "feedv2.experiments";
const char kEnableWebFeedFollowIntroDebug[] =
    "webfeed_follow_intro_debug.enable";
const char kReliabilityLoggingIdSalt[] = "feedv2.reliability_logging_id_salt";
const char kIsWebFeedSubscriber[] = "webfeed.is_subscriber";
const char kHasStoredData[] = "feedv2.has_stored_data";

}  // namespace prefs

// Deprecated prefs:
namespace {
// Deprecated 02/2021
const char kLastRefreshWasSignedIn[] = "feed.last_refresh_was_signed_in";
const char kBackgroundRefreshPeriod[] = "feed.background_refresh_period";
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

// Deprecated 05/2021 (can be removed along with 02/2021)
const char kEnableWebFeedUI[] = "webfeed_ui.enable";

void RegisterObsoletePrefsFeb_2021(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLastRefreshWasSignedIn, false);
  registry->RegisterTimeDeltaPref(kBackgroundRefreshPeriod, base::TimeDelta());
  registry->RegisterIntegerPref(kThrottlerRequestCount, 0);
  registry->RegisterIntegerPref(kThrottlerRequestsDay, 0);
  registry->RegisterDoublePref(kUserClassifierAverageSuggestionsViwedPerHour,
                               0.0);
  registry->RegisterDoublePref(kUserClassifierAverageSuggestionsUsedPerHour,
                               0.0);
  registry->RegisterTimePref(kUserClassifierLastTimeToViewSuggestions,
                             base::Time());
  registry->RegisterTimePref(kUserClassifierLastTimeToUseSuggestions,
                             base::Time());
  registry->RegisterBooleanPref(kEnableWebFeedUI, false);
}
}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(feed::prefs::kHostOverrideHost, "");
  registry->RegisterStringPref(feed::prefs::kHostOverrideBlessNonce, "");
  registry->RegisterTimePref(prefs::kLastFetchAttemptTime, base::Time());
  registry->RegisterListPref(feed::prefs::kThrottlerRequestCountListPrefName);
  registry->RegisterTimePref(feed::prefs::kThrottlerLastRequestTime,
                             base::Time());
  registry->RegisterStringPref(feed::prefs::kDebugStreamData, std::string());
  registry->RegisterDictionaryPref(feed::prefs::kRequestSchedule);
  registry->RegisterDictionaryPref(feed::prefs::kWebFeedsRequestSchedule);
  registry->RegisterDictionaryPref(feed::prefs::kMetricsData);
  registry->RegisterStringPref(feed::prefs::kClientInstanceId, "");
  registry->RegisterStringPref(feed::prefs::kDiscoverAPIEndpointOverride, "");
  registry->RegisterBooleanPref(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions, false);
  registry->RegisterBooleanPref(feed::prefs::kLastFetchHadNoticeCard, true);

  registry->RegisterIntegerPref(feed::prefs::kNoticeCardViewsCount, 0);
  registry->RegisterIntegerPref(feed::prefs::kNoticeCardClicksCount, 0);
  registry->RegisterDictionaryPref(feed::prefs::kExperiments);
  registry->RegisterBooleanPref(feed::prefs::kEnableWebFeedFollowIntroDebug,
                                false);
  registry->RegisterUint64Pref(feed::prefs::kReliabilityLoggingIdSalt, 0);
  registry->RegisterBooleanPref(feed::prefs::kIsWebFeedSubscriber, false);
  registry->RegisterBooleanPref(feed::prefs::kHasStoredData, false);

#if defined(OS_IOS)
  registry->RegisterBooleanPref(feed::prefs::kLastFetchHadLoggingEnabled,
                                false);
#endif  // defined(OS_IOS)

  RegisterObsoletePrefsFeb_2021(registry);
}

void MigrateObsoleteProfilePrefsFeb_2021(PrefService* prefs) {
  prefs->ClearPref(kLastRefreshWasSignedIn);
  prefs->ClearPref(kBackgroundRefreshPeriod);
  prefs->ClearPref(kThrottlerRequestCount);
  prefs->ClearPref(kThrottlerRequestsDay);
  prefs->ClearPref(kUserClassifierAverageSuggestionsViwedPerHour);
  prefs->ClearPref(kUserClassifierAverageSuggestionsUsedPerHour);
  prefs->ClearPref(kUserClassifierLastTimeToViewSuggestions);
  prefs->ClearPref(kUserClassifierLastTimeToUseSuggestions);
  prefs->ClearPref(kEnableWebFeedUI);
}

}  // namespace feed
