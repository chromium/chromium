// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/ios_promo_feature_configuration.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

absl::optional<FeatureConfig> GetClientSideiOSPromoFeatureConfig(
    const base::Feature* feature) {
  if (kIPHiOSAppStorePromoFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used =
        EventConfig("app_store_promo_used", Comparator(EQUAL, 0), 365, 730);
    config->trigger =
        EventConfig("app_store_promo_trigger", Comparator(EQUAL, 0), 365, 730);
    return config;
  }

  return absl::nullopt;
}

}  // namespace feature_engagement
