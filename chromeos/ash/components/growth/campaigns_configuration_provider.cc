// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_configuration_provider.h"

#include <cstring>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "chromeos/ash/components/growth/campaigns_utils.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/field_trial_utils.h"
#include "components/feature_engagement/public/group_list.h"

namespace growth {

namespace {

constexpr char kGrowthFramework[] = "ChromeOS Growth Framework";
constexpr char kGrowthCampaignsEventUsed[] =
    "ChromeOSAshGrowthCampaigns_EventUsed";
constexpr char kGrowthCampaignsEventTrigger[] =
    "ChromeOSAshGrowthCampaigns_EventTrigger";

feature_engagement::FeatureConfig CreateEmptyConfig() {
  feature_engagement::FeatureConfig config;
  config.valid = true;
  config.availability =
      feature_engagement::Comparator(feature_engagement::ANY, 0);
  config.session_rate =
      feature_engagement::Comparator(feature_engagement::ANY, 0);
  config.used = feature_engagement::EventConfig(
      kGrowthCampaignsEventUsed,
      feature_engagement::Comparator(feature_engagement::ANY, 0), 0, 0);
  config.trigger = feature_engagement::EventConfig(
      kGrowthCampaignsEventTrigger,
      feature_engagement::Comparator(feature_engagement::ANY, 0), 0, 0);
  return config;
}

bool HasDebugClearEventsSwitch() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kGrowthCampaignsClearEventsAtSessionStart);
}

}  // namespace

CampaignsConfigurationProvider::CampaignsConfigurationProvider() {
  SetConfig(CreateEmptyConfig());
}

CampaignsConfigurationProvider::~CampaignsConfigurationProvider() = default;

bool CampaignsConfigurationProvider::MaybeProvideFeatureConfiguration(
    const base::Feature& feature,
    feature_engagement::FeatureConfig& config,
    const feature_engagement::FeatureVector& known_features,
    const feature_engagement::GroupVector& known_groups) const {
  // Skip if it is not growth framework feature.
  if (std::strcmp((&feature_engagement::kIPHGrowthFramework)->name,
                  feature.name)) {
    return false;
  }

  // TODO: b/330533766 - Implement the matching logic.
  config = config_;
  return true;
}

const char* CampaignsConfigurationProvider::GetConfigurationSourceDescription()
    const {
  return kGrowthFramework;
}

std::set<std::string>
CampaignsConfigurationProvider::MaybeProvideAllowedEventPrefixes(
    const base::Feature& feature) const {
  if (std::strcmp((&feature_engagement::kIPHGrowthFramework)->name,
                  feature.name)) {
    return {};
  }

  // By returning empty prefixes, the `feature engagement` component will
  // clear all events with the `kGrowthCampaignsEventNamePrefix` and prevent
  // evaluating/recording the events with the prefix.
  // NOTE: To make growth framework events targeting work, need to remove the
  // debugging switch and restart the device again.
  if (HasDebugClearEventsSwitch()) {
    return {};
  } else {
    return {std::string(growth::GetGrowthCampaignsEventNamePrefix())};
  }
}

void CampaignsConfigurationProvider::SetConfig(
    std::map<std::string, std::string> params) {
  config_ = feature_engagement::FeatureConfig();
  uint32_t parse_errors = 0;
  feature_engagement::ConfigParseOutput output(parse_errors);
  output.trigger = &config_.trigger;
  output.used = &config_.used;
  output.event_configs = &config_.event_configs;

  feature_engagement::ParseConfigFields(
      &feature_engagement::kIPHGrowthFramework, params, output,
      /*known_features=*/{},
      /*known_groups=*/{});
  config_.valid = parse_errors == 0;
}

void CampaignsConfigurationProvider::SetConfig(
    const feature_engagement::FeatureConfig& config) {
  config_ = config;
}

}  // namespace growth
