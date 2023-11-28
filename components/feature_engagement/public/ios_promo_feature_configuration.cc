// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/ios_promo_feature_configuration.h"

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

namespace {

// Returns a config for a standard promo. This includes a rule for "only show
// this feature once every month."
absl::optional<FeatureConfig> GetStandardPromoConfig(
    const base::Feature* feature) {
  absl::optional<FeatureConfig> config;
  if (kIPHiOSPromoAppStoreFeature.name == feature->name) {
    // Should trigger once every 365 days.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used =
        EventConfig("app_store_promo_used", Comparator(EQUAL, 0), 365, 365);
    config->trigger =
        EventConfig("app_store_promo_trigger", Comparator(EQUAL, 0), 365, 365);
  }

  if (kIPHiOSPromoWhatsNewFeature.name == feature->name) {
    // Should trigger and display What's New when requested at most once a
    // month.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used =
        EventConfig("whats_new_promo_used", Comparator(ANY, 0), 365, 365);
    // What's New promo should be trigger no more than once a month.
    config->trigger = EventConfig("whats_new_promo_trigger",
                                  Comparator(LESS_THAN, 1), 30, 365);
  }

  if (kIPHiOSPromoDefaultBrowserFeature.name == feature->name) {
    // Should trigger at most 4 times in a year, and only after Chrome has
    // been opened 7 or more times.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used =
        EventConfig("default_browser_promo_used", Comparator(ANY, 0), 365, 365);
    if (base::FeatureList::IsEnabled(kDefaultBrowserEligibilitySlidingWindow)) {
      // Impression limits are currently being enforced on the registration side
      // of the promo manager on iOS, therefore this promo will not be showing
      // biweekly as this config may suggest.
      // TODO(b/302111496): Fix this config to have impression limits be
      // enforced solely by the FET.
      config->trigger = EventConfig("default_browser_promo_trigger",
                                    Comparator(EQUAL, 0), 14, 365);
    } else {
      config->trigger = EventConfig("default_browser_promo_trigger",
                                    Comparator(LESS_THAN, 4), 365, 365);
    }

    config->event_configs.insert(EventConfig(
        "chrome_opened", Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));
    // Default Browser promo shouldn't be shown if the Post Restore Default
    // Browser Promo has been shown in the past 7 days.
    config->event_configs.insert(
        EventConfig("post_restore_default_browser_promo_trigger",
                    Comparator(EQUAL, 0), 7, 365));
  }

  if (kIPHiOSPromoCredentialProviderExtensionFeature.name == feature->name) {
    // Should show no more than 3 times. Also, the promo is first shown in a
    // different form, and then shown via this feature one day later (after
    // snoozing).
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->used = EventConfig("credential_provider_extension_promo_used",
                               Comparator(ANY, 0), 365, 365);
    config->trigger = EventConfig("credential_provider_extension_promo_trigger",
                                  Comparator(LESS_THAN, 3), 365, 365);
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

  if (kIPHiOSPromoOmniboxPositionFeature.name == feature->name) {
    // Shown only once.
    config = FeatureConfig();
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("omnibox_position_promo_used",
                               Comparator(ANY, 0), 365, 365);
    config->trigger =
        EventConfig("omnibox_position_promo_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
  }

  // All standard promos can only be shown once per month.
  if (config) {
    config->event_configs.insert(
        EventConfig(config->trigger.name, Comparator(EQUAL, 0), 30, 365));
    return config;
  }
  return absl::nullopt;
}

// Returns a config for a custom feature that does not follow the standard
// rules.
absl::optional<FeatureConfig> GetCustomConfig(const base::Feature* feature) {
  if (kIPHiOSPromoPostRestoreFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used =
        EventConfig("post_restore_promo_used", Comparator(ANY, 0), 365, 365);
    // Should not be subject to impression limits, as it helps users recover
    // from being signed-out after restoring their device.
    config->trigger =
        EventConfig("post_restore_promo_trigger", Comparator(ANY, 0), 365, 365);
    return config;
  }

  if (kIPHiOSDefaultBrowserVideoPromoTriggerFeature.name == feature->name) {
    // A config for a pseudo feature strictly to keep track of some of the
    // criteria to register the default browser video promo with the promo
    // manager. This FET does not directly show the promo. Should trigger only
    // if default_browser_video_promo_conditions_met has been fired 1 or more
    // times in the last 2 weeks and the user did not see any other default
    // browser promo in the last 2 weeks.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;

    if (base::FeatureList::IsEnabled(kDefaultBrowserEligibilitySlidingWindow)) {
      config->trigger = EventConfig(
          "default_browser_video_promo_conditions_met_trigger",
          Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
          feature_engagement::kMaxStoragePeriod);
    } else {
      config->trigger =
          EventConfig("default_browser_video_promo_conditions_met_trigger",
                      Comparator(ANY, 0), 360, 360);
    }
    config->used = EventConfig("default_browser_video_promo_shown",
                               Comparator(EQUAL, 0), 360, 360);
    config->event_configs.insert(
        EventConfig("default_browser_video_promo_conditions_met",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 14, 360));
    config->event_configs.insert(EventConfig("default_browser_promo_shown",
                                             Comparator(EQUAL, 0), 14, 360));
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSPromoDefaultBrowserReminderFeature.name == feature->name) {
    // A config for a feature to handle re-showing the default browser promo
    // after a "Remind Me Later". Should trigger only if the reminder happened
    // over X days ago (i.e count == 0 in the past X days and count >= 1 in
    // general). The default configuration here allows snoozing once for 1 day,
    // but this can be changed via Finch.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("default_browser_promo_reminder_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("default_browser_promo_reminder_used",
                               Comparator(ANY, 0), 360, 360);
    config->event_configs.insert(EventConfig(
        "default_browser_promo_remind_me_later", Comparator(EQUAL, 0), 1, 360));
    config->event_configs.insert(
        EventConfig("default_browser_promo_remind_me_later",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 360, 360));
    return config;
  }

  if (kIPHiOSPromoPostRestoreDefaultBrowserFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("post_restore_default_browser_promo_used",
                               Comparator(ANY, 0), 365, 365);
    // Should not be subject to impression limits, as it helps users recover
    // from losing default browser status after restoring their device.
    config->trigger = EventConfig("post_restore_default_browser_promo_trigger",
                                  Comparator(ANY, 0), 365, 365);
    return config;
  }

  if (kIPHiOSChoiceScreenFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used =
        EventConfig("choice_screen_used", Comparator(ANY, 0), 365, 365);
    // Should not be subject to impression limits, as it is a choice the user
    // has to make.
    config->trigger =
        EventConfig("choice_screen_trigger", Comparator(ANY, 0), 365, 365);
    return config;
  }

  return absl::nullopt;
}
}  // namespace

absl::optional<FeatureConfig> GetClientSideiOSPromoFeatureConfig(
    const base::Feature* feature) {
  absl::optional<FeatureConfig> config = GetStandardPromoConfig(feature);
  if (config) {
    return config;
  }
  config = GetCustomConfig(feature);
  if (config) {
    return config;
  }
  return absl::nullopt;
}

}  // namespace feature_engagement
