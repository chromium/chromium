// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/config.h"

#include "base/containers/flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "components/feed/core/proto/v2/wire/capability.pb.h"
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
void OverrideWithFinch(Config* config) {
  config->max_feed_query_requests_per_day =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_feed_query_requests_per_day",
          config->max_feed_query_requests_per_day);

  config->max_action_upload_requests_per_day =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_action_upload_requests_per_day",
          config->max_action_upload_requests_per_day);

  config->stale_content_threshold =
      base::TimeDelta::FromSecondsD(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "stale_content_threshold_seconds",
          config->stale_content_threshold.InSecondsF()));

  config->default_background_refresh_interval =
      base::TimeDelta::FromSecondsD(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "default_background_refresh_interval_seconds",
          config->stale_content_threshold.InSecondsF()));

  config->max_action_upload_attempts = base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedV2, "max_action_upload_attempts",
      config->max_action_upload_attempts);

  config->max_action_age =
      base::TimeDelta::FromSecondsD(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "max_action_age_seconds",
          config->max_action_age.InSecondsF()));

  config->max_action_upload_bytes = base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedV2, "max_action_upload_bytes",
      config->max_action_upload_bytes);

  config->model_unload_timeout =
      base::TimeDelta::FromSecondsD(base::GetFieldTrialParamByFeatureAsDouble(
          kInterestFeedV2, "model_unload_timeout_seconds",
          config->model_unload_timeout.InSecondsF()));

  config->load_more_trigger_lookahead = base::GetFieldTrialParamByFeatureAsInt(
      kInterestFeedV2, "load_more_trigger_lookahead",
      config->load_more_trigger_lookahead);

  config->upload_actions_on_enter_background =
      base::GetFieldTrialParamByFeatureAsBool(
          kInterestFeedV2, "upload_actions_on_enter_background",
          config->upload_actions_on_enter_background);

  config->send_signed_out_session_logs =
      base::GetFieldTrialParamByFeatureAsBool(
          kInterestFeedV2, "send_signed_out_session_logs",
          config->send_signed_out_session_logs);

  config->session_id_max_age =
      base::TimeDelta::FromDays(base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "session_id_max_age_days",
          config->session_id_max_age.InDays()));

  config->max_prefetch_image_requests_per_refresh =
      base::GetFieldTrialParamByFeatureAsInt(
          kInterestFeedV2, "max_prefetch_image_requests_per_refresh",
          config->max_prefetch_image_requests_per_refresh);

  // Erase any capabilities with "enable_CAPABILITY = false" set.
  base::EraseIf(config->experimental_capabilities, CapabilityDisabled);
}

}  // namespace

const Config& GetFeedConfig() {
  static Config* s_config = nullptr;
  if (!s_config) {
    s_config = new Config;
    OverrideWithFinch(s_config);
  }
  return *s_config;
}

void SetFeedConfigForTesting(const Config& config) {
  const_cast<Config&>(GetFeedConfig()) = config;
}

void OverrideConfigWithFinchForTesting() {
  OverrideWithFinch(&const_cast<Config&>(GetFeedConfig()));
}

Config::Config() = default;
Config::Config(const Config& other) = default;
Config::~Config() = default;

}  // namespace feed
