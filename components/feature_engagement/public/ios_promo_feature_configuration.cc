// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/ios_promo_feature_configuration.h"

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

namespace {

// Returns a config for a standard promo. This includes a rule for "only show
// this feature once every month." Promos here can be unit tested in
// `PromosManagerFeatureEngagementTest`.
std::optional<FeatureConfig> GetStandardPromoConfig(
    const base::Feature* feature) {
  std::optional<FeatureConfig> config;
  if (kIPHiOSPromoAppStoreFeature.name == feature->name) {
    // Should trigger once every 365 days.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used =
        EventConfig("app_store_promo_used", Comparator(EQUAL, 0), 365, 365);
    config->trigger =
        EventConfig("app_store_promo_trigger", Comparator(EQUAL, 0), 365, 365);
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kChromeOpened,
                    Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));
    return config;
  }

  if (kIPHiOSPromoWhatsNewFeature.name == feature->name) {
    // Should trigger and display What's New when requested at most once a
    // month.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used =
        EventConfig("whats_new_promo_used", Comparator(ANY, 0), 365, 365);
    // What's New promo should be trigger no more than once every 14 days.
    config->trigger = EventConfig("whats_new_promo_trigger",
                                  Comparator(LESS_THAN, 1), 14, 365);
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kViewedWhatsNew,
                    Comparator(LESS_THAN, 1), 365, 365));
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kChromeOpened,
                    Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));
    return config;
  }

  if (kIPHiOSPromoGenericDefaultBrowserFeature.name == feature->name) {
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSDefaultBrowserPromosGroup.name);

    if (base::FeatureList::IsEnabled(kDefaultBrowserEligibilitySlidingWindow)) {
      // Show this promo once in number of days specified by the feature param.
      config->trigger = EventConfig(
          "generic_default_browser_promo_trigger", Comparator(EQUAL, 0),
          feature_engagement::kDefaultBrowserEligibilitySlidingWindowParam
              .Get(),
          feature_engagement::kMaxStoragePeriod);
    } else {
      // Show this promo only once.
      config->trigger = EventConfig("generic_default_browser_promo_trigger",
                                    Comparator(EQUAL, 0),
                                    feature_engagement::kMaxStoragePeriod,
                                    feature_engagement::kMaxStoragePeriod);
    }

    if (base::FeatureList::IsEnabled(
            kDefaultBrowserTriggerCriteriaExperiment)) {
      // Skip the regular conditions check for trigger criteria experiment and
      // check experiment specific condition(it has been enabled for at least 21
      // days).
      config->event_configs.insert(
          EventConfig("default_browser_promo_trigger_criteria_conditions_met",
                      Comparator(GREATER_THAN, 0), 365, 365));
    } else {
      // Show the promo if promo specific conditions are met during last 21
      // days.
      config->event_configs.insert(
          EventConfig("generic_default_browser_promo_conditions_met",
                      Comparator(GREATER_THAN, 0), 21, 365));
    }

    return config;
  }

  if (kIPHiOSPromoAllTabsFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config->groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);

    config->trigger =
        EventConfig("all_tabs_promo_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->event_configs.insert(EventConfig(
        "all_tabs_promo_conditions_met", Comparator(GREATER_THAN, 0), 21, 365));
    return config;
  }

  if (kIPHiOSPromoMadeForIOSFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config->groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);

    config->trigger =
        EventConfig("made_for_ios_promo_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->event_configs.insert(
        EventConfig("made_for_ios_promo_conditions_met",
                    Comparator(GREATER_THAN, 0), 21, 365));
    return config;
  }

  if (kIPHiOSPromoStaySafeFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config->groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);

    config->trigger =
        EventConfig("stay_safe_promo_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->event_configs.insert(EventConfig("stay_safe_promo_conditions_met",
                                             Comparator(GREATER_THAN, 0), 21,
                                             365));
    return config;
  }

  if (kIPHiOSPromoCredentialProviderExtensionFeature.name == feature->name) {
    // Should show no more than 3 times. Also, the promo is first shown in a
    // different form, and then shown via this feature one day later (after
    // snoozing).
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
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

  if (kIPHiOSDockingPromoFeature.name == feature->name) {
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("docking_promo_used", Comparator(EQUAL, 0),
                               feature_engagement::kMaxStoragePeriod,
                               feature_engagement::kMaxStoragePeriod);
    config->trigger = EventConfig("docking_promo_trigger", Comparator(EQUAL, 0),
                                  feature_engagement::kMaxStoragePeriod,
                                  feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSPostDefaultAbandonmentPromoFeature.name == feature->name) {
    config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config->used = EventConfig("post_default_abandonment_promo_used",
                               Comparator(ANY, 0), 365, 365);
    config->trigger =
        EventConfig("post_default_abandonment_promo_trigger",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  return std::nullopt;
}

// Returns a config for a custom feature that does not follow the standard
// rules.
std::optional<FeatureConfig> GetCustomConfig(const base::Feature* feature) {
  if (kIPHiOSPromoPostRestoreFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHWhatsNewUpdatedFeature.name == feature->name) {
    // Should trigger and display What's New badged only when What's New was not
    // viewed.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // This feature only affects badges, so shouldn't impact or block anything
    // else.
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    config->used =
        EventConfig("whats_new_updated_used", Comparator(ANY, 0), 365, 365);
    config->trigger =
        EventConfig("whats_new_updated_trigger", Comparator(ANY, 0), 365, 365);
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kViewedWhatsNew,
                    Comparator(LESS_THAN, 1), 365, 365));
    return config;
  }

  if (kIPHiOSPromoDefaultBrowserReminderFeature.name == feature->name) {
    // A config for a feature to handle re-showing the default browser promo
    // after a "Remind Me Later". Should trigger only if the reminder happened
    // over X days ago (i.e count == 0 in the past X days and count >= 1 in
    // general). The default configuration here allows snoozing once for 1 day,
    // but this can be changed via Finch.
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHiOSDockingPromoRemindMeLaterFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("docking_promo_remind_me_later_used",
                               Comparator(ANY, 0), 365, 365);
    config->trigger = EventConfig("docking_promo_remind_me_later_trigger",
                                  Comparator(ANY, 0), 365, 365);
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kDockingPromoRemindMeLater,
                    Comparator(LESS_THAN, 1), 3, 365));
    return config;
  }

  if (kIPHiOSSavedTabGroupClosed.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("saved_tab_group_closed_used",
                               Comparator(ANY, 0), 365, 365);
    config->trigger = EventConfig("saved_tab_group_closed_trigger",
                                  Comparator(EQUAL, 0), 365, 365);
    return config;
  }

  return std::nullopt;
}
}  // namespace

std::optional<FeatureConfig> GetClientSideiOSPromoFeatureConfig(
    const base::Feature* feature) {
  std::optional<FeatureConfig> config = GetStandardPromoConfig(feature);
  if (config) {
    // All standard promos can only be shown once per month, and must belong to
    // the full-screen promo group.
    config->event_configs.insert(
        EventConfig(config->trigger.name, Comparator(EQUAL, 0), 30, 365));
    config->groups.push_back(kiOSFullscreenPromosGroup.name);
    return config;
  }
  config = GetCustomConfig(feature);
  if (config) {
    return config;
  }
  return std::nullopt;
}

}  // namespace feature_engagement
