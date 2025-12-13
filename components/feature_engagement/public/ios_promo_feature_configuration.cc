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
  if (kIPHiOSPromoAppStoreFeature.name == feature->name) {
    // Should trigger once every 365 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("app_store_promo_used", Comparator(EQUAL, 0), 365, 365);
    config.trigger =
        EventConfig("app_store_promo_trigger", Comparator(EQUAL, 0), 365, 365);
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kChromeOpened,
                    Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));
    config.storage_type = StorageType::DEVICE;
    return config;
  }

  if (kIPHiOSPromoWhatsNewFeature.name == feature->name) {
    // Should trigger and display What's New when requested at most once a
    // month.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("whats_new_promo_used", Comparator(ANY, 0), 365, 365);
    // What's New promo should be trigger no more than once every 14 days.
    config.trigger = EventConfig("whats_new_promo_trigger",
                                 Comparator(LESS_THAN, 1), 14, 365);
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kViewedWhatsNew,
                    Comparator(LESS_THAN, 1), 365, 365));
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kChromeOpened,
                    Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));

    // Only show the promo if the Welcome Back Screen hasn't been displayed
    // in the past 3 days.
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kIOSWelcomeBackPromoTrigger,
                    Comparator(EQUAL, 0), 3, 365));

    return config;
  }

  if (kIPHiOSPromoGenericDefaultBrowserFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    if (base::FeatureList::IsEnabled(kDefaultBrowserEligibilitySlidingWindow)) {
      // Show this promo once in number of days specified by the feature param.
      config.trigger = EventConfig(
          "generic_default_browser_promo_trigger", Comparator(EQUAL, 0),
          feature_engagement::kDefaultBrowserEligibilitySlidingWindowParam
              .Get(),
          feature_engagement::kMaxStoragePeriod);
    } else {
      // Show this promo only once.
      config.trigger = EventConfig("generic_default_browser_promo_trigger",
                                   Comparator(EQUAL, 0),
                                   feature_engagement::kMaxStoragePeriod,
                                   feature_engagement::kMaxStoragePeriod);
    }

    // The off-cycle promo should count as a generic promo impression,
    // effectively putting it back on cooldown.
    config.event_configs.insert(EventConfig(
        "default_browser_off_cycle_promo_trigger", Comparator(EQUAL, 0),
        feature_engagement::kDefaultBrowserEligibilitySlidingWindowParam.Get(),
        feature_engagement::kMaxStoragePeriod));

      // Show the promo if promo specific conditions are met during last 21
      // days.
      config.event_configs.insert(
          EventConfig("generic_default_browser_promo_conditions_met",
                      Comparator(GREATER_THAN, 0), 21, 365));

    return config;
  }

  if (kIPHiOSPromoAllTabsFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    config.trigger = EventConfig("all_tabs_promo_trigger", Comparator(EQUAL, 0),
                                 feature_engagement::kMaxStoragePeriod,
                                 feature_engagement::kMaxStoragePeriod);
    config.event_configs.insert(EventConfig(
        "all_tabs_promo_conditions_met", Comparator(GREATER_THAN, 0), 21, 365));
    return config;
  }

  if (kIPHiOSPromoMadeForIOSFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    config.trigger =
        EventConfig("made_for_ios_promo_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.event_configs.insert(EventConfig("made_for_ios_promo_conditions_met",
                                            Comparator(GREATER_THAN, 0), 21,
                                            365));
    return config;
  }

  if (kIPHiOSPromoStaySafeFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    config.trigger =
        EventConfig("stay_safe_promo_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.event_configs.insert(EventConfig("stay_safe_promo_conditions_met",
                                            Comparator(GREATER_THAN, 0), 21,
                                            365));
    return config;
  }

  if (kIPHiOSPromoCredentialProviderExtensionFeature.name == feature->name) {
    // Should show no more than 3 times. Also, the promo is first shown in a
    // different form, and then shown via this feature one day later (after
    // snoozing).
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("credential_provider_extension_promo_used",
                              Comparator(ANY, 0), 365, 365);
    config.trigger = EventConfig("credential_provider_extension_promo_trigger",
                                 Comparator(LESS_THAN, 3), 365, 365);
    // To track the fake snoozing, the snooze event must have happened once, but
    // not in the past day. This acts as waiting for a day to display the promo.
    config.event_configs.insert(
        EventConfig("credential_provider_extension_promo_snoozed",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 365, 365));
    config.event_configs.insert(
        EventConfig("credential_provider_extension_promo_snoozed",
                    Comparator(EQUAL, 0), 1, 365));
    return config;
  }

  if (kIPHiOSDockingPromoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("docking_promo_used", Comparator(EQUAL, 0),
                              feature_engagement::kMaxStoragePeriod,
                              feature_engagement::kMaxStoragePeriod);
    config.trigger = EventConfig("docking_promo_trigger", Comparator(EQUAL, 0),
                                 feature_engagement::kMaxStoragePeriod,
                                 feature_engagement::kMaxStoragePeriod);
    config.storage_type = StorageType::DEVICE;
    return config;
  }

  if (kIPHiOSPostDefaultAbandonmentPromoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.used = EventConfig("post_default_abandonment_promo_used",
                              Comparator(ANY, 0), 365, 365);
    config.trigger =
        EventConfig("post_default_abandonment_promo_trigger",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSPromoSigninFullscreenFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("signin_fullscreen_promo_used", Comparator(ANY, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.trigger = EventConfig(
        feature_engagement::events::kIOSSigninFullscreenPromoTrigger,
        Comparator(ANY, 0), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSWelcomeBackFeature.name == feature->name) {
    // Show the promo any time the conditions are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig(feature_engagement::events::kIOSWelcomeBackPromoUsed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.trigger =
        EventConfig(feature_engagement::events::kIOSWelcomeBackPromoTrigger,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.storage_type = StorageType::DEVICE;
    return config;
  }

  return std::nullopt;
}

// Returns a config for a custom feature that does not follow the standard
// rules.
std::optional<FeatureConfig> GetCustomConfig(const base::Feature* feature) {
  if (kIPHiOSPromoPostRestoreFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("post_restore_promo_used", Comparator(ANY, 0), 365, 365);
    // Should not be subject to impression limits, as it helps users recover
    // from being signed-out after restoring their device.
    config.trigger =
        EventConfig("post_restore_promo_trigger", Comparator(ANY, 0), 365, 365);
    config.storage_type = StorageType::DEVICE;
    return config;
  }

  if (kIPHWhatsNewUpdatedFeature.name == feature->name) {
    // Should trigger and display What's New badged only when What's New was not
    // viewed.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // This feature only affects badges, so shouldn't impact or block anything
    // else.
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    config.used =
        EventConfig("whats_new_updated_used", Comparator(ANY, 0), 365, 365);
    config.trigger =
        EventConfig("whats_new_updated_trigger", Comparator(ANY, 0), 365, 365);
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kViewedWhatsNew,
                    Comparator(LESS_THAN, 1), 365, 365));
    config.storage_type = StorageType::DEVICE;
    return config;
  }

  if (kIPHiOSPromoDefaultBrowserReminderFeature.name == feature->name) {
    // A config for a feature to handle re-showing the default browser promo
    // after a "Remind Me Later". Should trigger only if the reminder happened
    // over X days ago (i.e count == 0 in the past X days and count >= 1 in
    // general). The default configuration here allows snoozing once for 1 day,
    // but this can be changed via Finch.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("default_browser_promo_reminder_trigger",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig("default_browser_promo_reminder_used",
                              Comparator(ANY, 0), 360, 360);
    config.event_configs.insert(EventConfig(
        "default_browser_promo_remind_me_later", Comparator(EQUAL, 0), 1, 360));
    config.event_configs.insert(
        EventConfig("default_browser_promo_remind_me_later",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 360, 360));
    return config;
  }

  if (kIPHiOSPromoPostRestoreDefaultBrowserFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("post_restore_default_browser_promo_used",
                              Comparator(ANY, 0), 365, 365);
    // Should not be subject to impression limits, as it helps users recover
    // from losing default browser status after restoring their device.
    config.trigger = EventConfig("post_restore_default_browser_promo_trigger",
                                 Comparator(ANY, 0), 365, 365);
    return config;
  }

  if (kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("non_modal_default_browser_promo_omnibox_paste_used",
                    Comparator(ANY, 0), 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger = EventConfig(
        feature_engagement::events::kNonModalDefaultBrowserPromoUrlPasteTrigger,
        Comparator(LESS_THAN, 1), 14, 365);
    // Should be triggered no more than twice (2x) every 6 months.
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kNonModalDefaultBrowserPromoUrlPasteTrigger,
        Comparator(LESS_THAN, 2), 180, 365));

    config.groups.push_back(kiOSTailoredNonModalDefaultBrowserPromosGroup.name);
    return config;
  }

  if (kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature.name ==
      feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("non_modal_default_browser_promo_app_switcher_used",
                    Comparator(ANY, 0), 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger =
        EventConfig(feature_engagement::events::
                        kNonModalDefaultBrowserPromoAppSwitcherTrigger,
                    Comparator(LESS_THAN, 1), 14, 365);
    // Should be triggered no more than twice (2x) every 6 months.
    config.event_configs.insert(
        EventConfig(feature_engagement::events::
                        kNonModalDefaultBrowserPromoAppSwitcherTrigger,
                    Comparator(LESS_THAN, 2), 180, 365));

    config.groups.push_back(kiOSTailoredNonModalDefaultBrowserPromosGroup.name);
    return config;
  }

  if (kIPHiOSPromoNonModalShareDefaultBrowserFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("non_modal_default_browser_promo_share_used",
                              Comparator(ANY, 0), 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger = EventConfig(
        feature_engagement::events::kNonModalDefaultBrowserPromoShareTrigger,
        Comparator(LESS_THAN, 1), 14, 365);
    // Should be triggered no more than twice (2x) every 6 months.
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kNonModalDefaultBrowserPromoShareTrigger,
        Comparator(LESS_THAN, 2), 180, 365));

    config.groups.push_back(kiOSTailoredNonModalDefaultBrowserPromosGroup.name);
    return config;
  }

  if (kIPHiOSPromoNonModalSigninPasswordFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);

    config.used = EventConfig("non_modal_signin_promo_password",
                              Comparator(ANY, 0), 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger = EventConfig(
        feature_engagement::events::kNonModalSigninPromoPasswordTrigger,
        Comparator(LESS_THAN, 1), 14, 365);

    config.groups.push_back(kiOSNonModalSigninPromosGroup.name);
    return config;
  }

  if (kIPHiOSPromoNonModalSigninBookmarkFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);

    config.used = EventConfig("non_modal_signin_promo_bookmark",
                              Comparator(ANY, 0), 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger = EventConfig(
        feature_engagement::events::kNonModalSigninPromoBookmarkTrigger,
        Comparator(LESS_THAN, 1), 14, 365);

    config.groups.push_back(kiOSNonModalSigninPromosGroup.name);
    return config;
  }

  if (kIPHiOSDockingPromoRemindMeLaterFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("docking_promo_remind_me_later_used",
                              Comparator(ANY, 0), 365, 365);
    config.trigger = EventConfig("docking_promo_remind_me_later_trigger",
                                 Comparator(ANY, 0), 365, 365);
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kDockingPromoRemindMeLater,
                    Comparator(LESS_THAN, 1), 3, 365));
    return config;
  }

  if (kIPHiOSSavedTabGroupClosed.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("saved_tab_group_closed_used", Comparator(ANY, 0),
                              365, 365);
    config.trigger = EventConfig("saved_tab_group_closed_trigger",
                                 Comparator(EQUAL, 0), 365, 365);
    return config;
  }

  if (kIPHiOSSharedTabGroupForeground.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("shared_tab_group_foreground_used",
                              Comparator(ANY, 0), 365, 365);
    config.trigger = EventConfig("shared_tab_group_foreground_trigger",
                                 Comparator(EQUAL, 0), 365, 365);
    return config;
  }

  if (kIPHiOSDefaultBrowserBannerPromoFeature.name == feature->name) {
    // Promo should show only once, and also require time since other promos.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.used = EventConfig("default_browser_banner_promo_used",
                              Comparator(ANY, 0), 365, 365);
    config.trigger =
        EventConfig("default_browser_banner_promo_trigger",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // This promo counts as a default browser promo despite not being a
    // fullscreen promo from the promos manager because it's still a
    // non-contextual default browser promo. Thus, it should share cooldown
    // rules.
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    return config;
  }

  if (kIPHiOSDefaultBrowserOffCyclePromoFeature.name == feature->name) {
    // A config for a feature to handle the off-cycle generic default browser
    // promo.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config->storage_type = StorageType::DEVICE;
    config->used = EventConfig("default_browser_off_cycle_promo_used",
                               Comparator(ANY, 0), 365, 365);

    config->trigger = EventConfig(
        "default_browser_off_cycle_promo_trigger", Comparator(EQUAL, 0),
        feature_engagement::kIPHiOSDefaultBrowserOffCyclePromoCooldown.Get(),
        feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSSafariImportFeature.name == feature->name) {
    // A config that shows the Safari import entry point modal. If the user
    // proceeds with the import or dismisses the modal, the entry point will
    // show again.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Can be triggered any number of times, as long as the user keeps setting
    // the reminder.
    config.trigger =
        EventConfig("ios_safari_import_entry_point_trigger", Comparator(ANY, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    // If the user has started or dismissed the Safari import workflow, don't
    // show the entry point again.
    config.used =
        EventConfig("ios_safari_import_entry_point_used_or_dismissed",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // Show the entry point if the user has neither started or dismissed the
    // Safari import workflow, nor tapped "remind me later" in the last two
    // days.
    config.event_configs.insert(EventConfig(
        events::kIOSSafariImportRemindMeLater, Comparator(EQUAL, 0), 2, 2));
    config.storage_type = StorageType::DEVICE;
    return config;
  }

  if (kIPHiOSOneTimeDefaultBrowserNotificationFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.storage_type = StorageType::DEVICE;
    config.trigger =
        EventConfig("one_time_default_browser_notification_trigger",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.event_configs.insert(EventConfig(
        "default_browser_promos_group_trigger", Comparator(EQUAL, 0), 14, 360));
    config.event_configs.insert(
        EventConfig("default_browser_fre_shown", Comparator(EQUAL, 0), 7, 365));
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
