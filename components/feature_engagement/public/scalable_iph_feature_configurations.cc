// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/scalable_iph_feature_configurations.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {
namespace {
absl::optional<FeatureConfig> GetBaseConfig() {
  absl::optional<FeatureConfig> config = FeatureConfig();
  config->valid = true;
  config->availability = Comparator(ANY, 0);
  config->session_rate = Comparator(ANY, 0);
  config->session_rate_impact.type = SessionRateImpact::Type::NONE;
  config->blocked_by.type = BlockedBy::Type::NONE;
  config->blocking.type = Blocking::Type::NONE;
  return config;
}

bool IsTrackingOnly() {
  return ash::features::IsScalableIphTrackingOnlyEnabled();
}
}  // namespace

absl::optional<FeatureConfig> GetScalableIphFeatureConfig(
    const base::Feature* feature) {
  if (!ash::features::IsScalableIphClientConfigEnabled()) {
    return absl::nullopt;
  }

  if (kIPHScalableIphHelpAppBasedOneFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = GetBaseConfig();
    config->used =
        EventConfig(scalable_iph::kEventNameHelpAppActionTypeOpenChrome,
                    Comparator(ANY, 0), 7, 8);
    config->trigger = EventConfig("ScalableIphHelpAppBasedOneTriggerNotUsed",
                                  Comparator(EQUAL, 0), 7, 8);

    config->tracking_only = IsTrackingOnly();
    return config;
  }

  // TODO(b/308010596): Move other config.

  return absl::nullopt;
}

}  // namespace feature_engagement
