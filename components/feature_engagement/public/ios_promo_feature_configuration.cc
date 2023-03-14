// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/ios_promo_feature_configuration.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

absl::optional<FeatureConfig> GetClientSideiOSPromoFeatureConfig(
    const base::Feature* feature) {
  if (kIPHiOSPromoAppStoreFeature.name == feature->name) {
    // Should trigger once every 365 days.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used =
        EventConfig("app_store_promo_used", Comparator(EQUAL, 0), 365, 365);
    config->trigger =
        EventConfig("app_store_promo_trigger", Comparator(EQUAL, 0), 365, 365);
    return config;
  }

  if (kIPHiOSPromoWhatsNewFeature.name == feature->name) {
    // Should trigger once only, and only after Chrome has been opened 6 or more
    // times.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used =
        EventConfig("whats_new_promo_used", Comparator(EQUAL, 0), 365, 365);
    // What's New promo should only ever trigger once.
    config->trigger = EventConfig("whats_new_promo_trigger",
                                  Comparator(EQUAL, 0), 1000, 1000);
    config->event_configs.insert(EventConfig(
        "chrome_opened", Comparator(GREATER_THAN_OR_EQUAL, 6), 365, 365));
    return config;
  }

  if (kIPHiOSPromoPostRestoreFeature.name == feature->name) {
    // Should always trigger when asked, as it helps users recover from being
    // signed-out after restoring their device.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used =
        EventConfig("post_restore_promo_used", Comparator(EQUAL, 0), 365, 365);
    // Post Restore promo should always show when requested.
    config->trigger =
        EventConfig("post_restore_promo_trigger", Comparator(ANY, 0), 365, 365);
    return config;
  }

  if (kIPHiOSPromoCredentialProviderExtensionFeature.name == feature->name) {
    // Should show no more than 3 times. Also, the promo is first shown in a
    // different form, and then shown via this feature one day later (after
    // snoozing).
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used = EventConfig("credential_provider_extension_promo_used",
                               Comparator(EQUAL, 0), 365, 365);
    config->trigger = EventConfig("credential_provider_extension_promo_trigger",
                                  Comparator(LESS_THAN, 3), 365, 365);
    // Standard constraint of no more than once per month.
    config->event_configs.insert(
        EventConfig("credential_provider_extension_promo_trigger",
                    Comparator(LESS_THAN, 1), 30, 365));
    // To track the fake snoozing, the snooze event must have happened once, but
    // not in the past day. This acts as waiting for a day to display the promo.
    config->event_configs.insert(
        EventConfig("credential_provider_extension_promo_snoozed",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 365, 365));
    config->event_configs.insert(
        EventConfig("credential_provider_extension_promo_snoozed",
                    Comparator(EQUAL, 0), 1, 365));
    return config;
  }

  return absl::nullopt;
}

}  // namespace feature_engagement
