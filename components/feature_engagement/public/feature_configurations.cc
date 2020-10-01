/// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_configurations.h"

#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

base::Optional<FeatureConfig> GetClientSideFeatureConfig(
    const base::Feature* feature) {
#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
  if (kIPHPasswordsAccountStorageFeature.name == feature->name) {
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("passwords_account_storage_trigger",
                                  Comparator(LESS_THAN, 5), 180, 180);
    config->used = EventConfig("passwords_account_storage_used",
                               Comparator(EQUAL, 0), 180, 180);
    config->event_configs.insert(
        EventConfig("passwords_account_storage_unselected",
                    Comparator(EQUAL, 0), 180, 180));
    return config;
  }
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  if (kIPHDataSaverDetailFeature.name == feature->name) {
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("data_saver_detail_iph_trigger",
                                  Comparator(EQUAL, 0), 90, 360);
    config->used = EventConfig("data_saver_overview_opened",
                               Comparator(EQUAL, 0), 90, 360);
    config->event_configs.insert(
        EventConfig("data_saved_page_load",
                    Comparator(GREATER_THAN_OR_EQUAL, 10), 90, 360));
    return config;
  }
  if (kIPHDataSaverPreviewFeature.name == feature->name) {
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("data_saver_preview_iph_trigger",
                                  Comparator(EQUAL, 0), 90, 360);
    config->used = EventConfig("data_saver_preview_opened",
                               Comparator(LESS_THAN_OR_EQUAL, 1), 90, 360);
    return config;
  }
  if (kIPHPreviewsOmniboxUIFeature.name == feature->name) {
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("previews_verbose_iph_triggered_2",
                                  Comparator(LESS_THAN, 2), 90, 360);
    config->used = EventConfig("previews_verbose_status_opened",
                               Comparator(EQUAL, 0), 90, 360);
    config->event_configs.insert(EventConfig(
        "preview_page_load", Comparator(GREATER_THAN_OR_EQUAL, 1), 90, 360));
    return config;
  }
  if (kIPHDownloadHomeFeature.name == feature->name) {
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("download_home_iph_trigger", Comparator(EQUAL, 0), 90, 360);
    config->used =
        EventConfig("download_home_opened", Comparator(EQUAL, 0), 90, 360);
    config->event_configs.insert(EventConfig(
        "download_completed", Comparator(GREATER_THAN_OR_EQUAL, 1), 90, 360));
    return config;
  }
  if (kIPHDownloadIndicatorFeature.name == feature->name) {
    // A config that allows the DownloadIndicator IPH to be shown up to 2 times,
    // but only if download home hasn't been opened in the last 90 days.
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("download_indicator_iph_trigger",
                                  Comparator(LESS_THAN, 2), 360, 360);
    config->used =
        EventConfig("download_home_opened", Comparator(EQUAL, 0), 90, 360);
    return config;
  }
  if (kIPHExploreSitesTileFeature.name == feature->name) {
    // A config that allows the ExploreSites IPH to be shown:
    // * Once per day
    // * Up to 3 times but only if unused in the last 90 days.
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("explore_sites_tile_iph_trigger",
                                  Comparator(LESS_THAN, 3), 90, 360);
    config->used =
        EventConfig("explore_sites_tile_tapped", Comparator(EQUAL, 0), 90, 360);
    config->event_configs.insert(EventConfig("explore_sites_tile_iph_trigger",
                                             Comparator(LESS_THAN, 1), 1, 360));
    return config;
  }
#endif  // defined(OS_ANDROID)

  if (kIPHDummyFeature.name == feature->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    base::Optional<FeatureConfig> config = FeatureConfig();
    config->valid = false;
    config->availability = Comparator(LESS_THAN, 0);
    config->session_rate = Comparator(LESS_THAN, 0);
    config->trigger = EventConfig("dummy_feature_iph_trigger",
                                  Comparator(LESS_THAN, 0), 1, 1);
    config->used =
        EventConfig("dummy_feature_action", Comparator(LESS_THAN, 0), 1, 1);
    return config;
  }

  return base::nullopt;
}

}  // namespace feature_engagement
