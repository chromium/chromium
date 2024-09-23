// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/config.h"

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/feed_feature_list.h"

namespace feed {
namespace {
// A note about the design.
// Soon, we'll add the ability to override configuration values from sources
// other than Finch. Finch serves well for experimentation, but after we're done
// experimenting, we still want to control some of these values. The tentative
// plan is to send configuration down from the server, and store it in prefs.
// The source of a config value would be the following, in order of preference:
// finch, server, default-value.

bool CapabilityDisabled(feedwire::Capability capability) {
  return !base::GetFieldTrialParamByFeatureAsBool(
      kInterestFeedV2,
      base::StrCat({"enable_", feedwire::Capability_Name(capability)}), true);
}

// Override any parameters that may be provided by Finch.
void OverrideWithFinch(Config& config) {
  config.max_feed_query_requests_per_day =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_feed_query_requests_per_day",
          config.max_feed_query_requests_per_day);

  config.max_next_page_requests_per_day =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_next_page_requests_per_day",
          config.max_next_page_requests_per_day);

  config.max_action_upload_requests_per_day =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_action_upload_requests_per_day",
          config.max_action_upload_requests_per_day);

  config.stale_content_threshold =
      base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "stale_content_threshold_seconds",
          config.stale_content_threshold.InSecondsF()));

  config.content_expiration_threshold =
      base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "content_expiration_threshold_seconds",
          config.content_expiration_threshold.InSecondsF()));

  if (base::FeatureList::IsEnabled(kWebFeedOnboarding)) {
    config.subscriptionless_content_expiration_threshold =
        base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
            kWebFeedOnboarding,
            "subscriptionless_content_expiration_threshold_seconds",
            config.subscriptionless_content_expiration_threshold.InSecondsF()));
  }

  config.background_refresh_window_length =
      base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "background_refresh_window_length_seconds",
          config.background_refresh_window_length.InSecondsF()));

  config.default_background_refresh_interval =
      base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "default_background_refresh_interval_seconds",
          config.default_background_refresh_interval.InSecondsF()));

  config.max_action_upload_attempts = base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedV2, "max_action_upload_attempts",
      config.max_action_upload_attempts);

  config.max_action_age =
      base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "max_action_age_seconds",
          config.max_action_age.InSecondsF()));

  config.max_action_upload_bytes = base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedV2, "max_action_upload_bytes",
      config.max_action_upload_bytes);

  config.model_unload_timeout =
      base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "model_unload_timeout_seconds",
          config.model_unload_timeout.InSecondsF()));

  config.load_more_trigger_lookahead = base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedV2, "load_more_trigger_lookahead",
      config.load_more_trigger_lookahead);

  config.load_more_trigger_scroll_distance_dp =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2Scrolling, "load_more_trigger_scroll_distance_dp",
          config.load_more_trigger_scroll_distance_dp);

  config.upload_actions_on_enter_background =
      base::GetFieldTrialParamByFeatureAsBool(
          kInterestFeedV2, "upload_actions_on_enter_background",
          config.upload_actions_on_enter_background);

  config.send_signed_out_session_logs = base::GetFieldTrialParamByFeatureAsBool(
      kInterestFeedV2, "send_signed_out_session_logs",
      config.send_signed_out_session_logs);

  config.session_id_max_age = base::Days(base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedV2, "session_id_max_age_days",
      config.session_id_max_age.InDays()));

  config.max_prefetch_image_requests_per_refresh =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_prefetch_image_requests_per_refresh",
          config.max_prefetch_image_requests_per_refresh);

  if (base::FeatureList::IsEnabled(kWebFeedOnboarding)) {
    config.subscriptionless_web_feed_stale_content_threshold =
        base::Seconds(base::GetFieldTrialParamByFeatureAsDouble(
            kWebFeedOnboarding,
            "subscriptionless_web_feed_stale_content_threshold_seconds",
            config.subscriptionless_web_feed_stale_content_threshold
                .InSecondsF()));
  }

  // Erase any capabilities with "enable_CAPABILITY = false" set.
  base::EraseIf(config.experimental_capabilities, CapabilityDisabled);

  config.max_mid_entities_per_url_entry =
      base::GetFieldTrialParamByFeatureAsInt(
          kPersonalizeFeedUnsignedUsers, "max_mid_entities_per_url_entry",
          config.max_mid_entities_per_url_entry);
  config.max_url_entries_in_cache = GetFieldTrialParamByFeatureAsInt(
      kPersonalizeFeedUnsignedUsers, "max_url_entries_in_cache",
      config.max_url_entries_in_cache);
}

void OverrideWithSwitches(Config& config) {
  config.use_feed_query_requests =
      base::CommandLine::ForCurrentProcess()->HasSwitch("use-legacy-feedquery");
}

}  // namespace

const Config& GetFeedConfig() {
  static Config* s_config = nullptr;
  if (!s_config) {
    s_config = new Config;
    OverrideWithFinch(*s_config);
    OverrideWithSwitches(*s_config);
  }
  return *s_config;
}

// This is a dev setting that updates Config, which is supposed to be constant.
void SetUseFeedQueryRequests(const bool use_legacy) {
  Config& config = const_cast<Config&>(GetFeedConfig());
  config.use_feed_query_requests = use_legacy;
}

void SetFeedConfigForTesting(const Config& config) {
  const_cast<Config&>(GetFeedConfig()) = config;
}

void OverrideConfigWithFinchForTesting() {
  OverrideWithFinch(const_cast<Config&>(GetFeedConfig()));
}

Config::Config() = default;
Config::Config(const Config& other) = default;
Config::~Config() = default;

base::TimeDelta Config::GetStalenessThreshold(const StreamType& stream_type,
                                              bool has_subscriptions) const {
  if (stream_type.IsForYou()) {
    return stale_content_threshold;
  }
  if (has_subscriptions) {
    return web_feed_stale_content_threshold;
  }
  return subscriptionless_web_feed_stale_content_threshold;
}

}  // namespace feed
