// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/field_trial_configuration_provider.h"

#include "base/metrics/field_trial_params.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/field_trial_utils.h"
#include "components/feature_engagement/public/group_constants.h"
#include "components/feature_engagement/public/group_list.h"
#include "components/feature_engagement/public/stats.h"

namespace feature_engagement {

FieldTrialConfigurationProvider::FieldTrialConfigurationProvider() = default;
FieldTrialConfigurationProvider::~FieldTrialConfigurationProvider() = default;

bool FieldTrialConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    FeatureConfig& config,
    const FeatureVector& known_features,
    const GroupVector& known_groups) const {
  if (config.valid) {
    return false;
  }

  base::FieldTrialParams params;
  if (!base::GetFieldTrialParamsByFeature(feature, &params) || params.empty()) {
    return false;
  }

  uint32_t parse_errors = 0;

  ConfigParseOutput output(parse_errors);
  output.session_rate = &config.session_rate;
  output.session_rate_impact = &config.session_rate_impact;
  output.blocking = &config.blocking;
  output.blocked_by = &config.blocked_by;
  output.trigger = &config.trigger;
  output.used = &config.used;
  output.event_configs = &config.event_configs;
  output.tracking_only = &config.tracking_only;
  output.availability = &config.availability;
  output.snooze_params = &config.snooze_params;
  output.groups = &config.groups;

  ParseConfigFields(&feature, params, output, known_features, known_groups);

  // The |used| and |trigger| members are required, so should not be the
  // default values.
  const bool has_used_event = config.used != EventConfig();
  const bool has_trigger_event = config.trigger != EventConfig();
  config.valid = has_used_event && has_trigger_event && parse_errors == 0;

  // Notice parse errors for used and trigger events will also cause the
  // following histograms being recorded.
  if (!has_used_event) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_USED_EVENT_MISSING);
  }
  if (!has_trigger_event) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_MISSING);
  }

  return true;
}

bool FieldTrialConfigurationProvider::MaybeProvideGroupConfiguration(
    const base::Feature& feature,
    GroupConfig& config) const {
  if (config.valid) {
    return false;
  }

  base::FieldTrialParams params;
  if (!base::GetFieldTrialParamsByFeature(feature, &params) || params.empty()) {
    return false;
  }

  uint32_t parse_errors = 0;

  ConfigParseOutput output(parse_errors);
  output.session_rate = &config.session_rate;
  output.trigger = &config.trigger;
  output.event_configs = &config.event_configs;

  ParseConfigFields(&feature, params, output, {}, {});

  // The |trigger| member is required, so should not be the
  // default value.
  const bool has_trigger_event = config.trigger != EventConfig();
  config.valid = has_trigger_event && parse_errors == 0;

  // Notice parse errors for trigger event will also cause the
  // following histogram to be recorded.
  if (!has_trigger_event) {
    stats::RecordConfigParsingEvent(
        stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_MISSING);
  }

  return true;
}

const char* FieldTrialConfigurationProvider::GetConfigurationSourceDescription()
    const {
  return "field trial";
}

}  // namespace feature_engagement
