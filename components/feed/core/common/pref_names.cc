// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/common/pref_names.h"

#include <string>

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace feed {

namespace prefs {

const char kHostOverrideHost[] = "feed.host_override.host";
const char kHostOverrideBlessNonce[] = "feed.host_override.bless_nonce";

const char kHasReachedClickAndViewActionsUploadConditions[] =
    "feed.clicks_and_views_upload_conditions_reached";
const char kLastFetchHadNoticeCard[] = "feed.last_fetch_had_notice_card";
#if BUILDFLAG(IS_IOS)
const char kLastFetchHadLoggingEnabled[] =
    "feed.last_fetch_had_logging_enabled";
#endif  // BUILDFLAG(IS_IOS)
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
const char kEnableWebFeedFollowIntroDebug[] =
    "webfeed_follow_intro_debug.enable";
const char kReliabilityLoggingIdSalt[] = "feedv2.reliability_logging_id_salt";
const char kHasStoredData[] = "feedv2.has_stored_data";
const char kWebFeedContentOrder[] = "webfeed.content_order";
const char kLastSeenFeedType[] = "feedv2.last_seen_feed_type";
const char kFeedOnDeviceUserActionsCollector[] = "feed.user_actions_collection";
const char kInfoCardStates[] = "feed.info_card_states";
const char kHasSeenWebFeed[] = "webfeed.has_seen_feed";
const char kLastBadgeAnimationTime[] = "webfeed.last_badge_animation_time";
const char kExperimentsV2[] = "feedv2.experiments_v2";

// Deprecated October 2022
const char kExperimentsDeprecated[] = "feedv2.experiments";

}  // namespace prefs

// Deprecated prefs:
namespace {

void RegisterObsoletePrefsOct_2022(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kExperimentsDeprecated);
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(feed::prefs::kHostOverrideHost, "");
  registry->RegisterStringPref(feed::prefs::kHostOverrideBlessNonce, "");
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
  registry->RegisterBooleanPref(feed::prefs::kEnableWebFeedFollowIntroDebug,
                                false);
  registry->RegisterUint64Pref(feed::prefs::kReliabilityLoggingIdSalt, 0);
  registry->RegisterBooleanPref(feed::prefs::kHasStoredData, false);
  registry->RegisterIntegerPref(feed::prefs::kWebFeedContentOrder, 0);
  registry->RegisterIntegerPref(feed::prefs::kLastSeenFeedType, 0);
  registry->RegisterListPref(feed::prefs::kFeedOnDeviceUserActionsCollector,
                             PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(feed::prefs::kInfoCardStates, 0);
  registry->RegisterBooleanPref(feed::prefs::kHasSeenWebFeed, false);
  registry->RegisterTimePref(feed::prefs::kLastBadgeAnimationTime,
                             base::Time());
  registry->RegisterDictionaryPref(feed::prefs::kExperimentsV2);

#if BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(feed::prefs::kLastFetchHadLoggingEnabled,
                                false);
#endif  // BUILDFLAG(IS_IOS)

  RegisterObsoletePrefsOct_2022(registry);
}

void MigrateObsoleteProfilePrefsOct_2022(PrefService* prefs) {
  const base::Value* val =
      prefs->GetUserPrefValue(prefs::kExperimentsDeprecated);
  const base::Value::Dict* old = val ? val->GetIfDict() : nullptr;
  if (old) {
    base::Value::Dict dict;
    for (const auto kv : *old) {
      base::Value::List list;
      list.Append(kv.second.GetString());
      dict.Set(kv.first, std::move(list));
    }
    prefs->SetDict(feed::prefs::kExperimentsV2, std::move(dict));
  }
  prefs->ClearPref(prefs::kExperimentsDeprecated);
}

}  // namespace feed
