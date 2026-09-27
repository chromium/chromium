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
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used =
        EventConfig("app_store_promo_used", Comparator(EQUAL, 0), 365, 365);
    config.trigger =
        EventConfig("app_store_promo_trigger", Comparator(EQUAL, 0), 365, 365);
    config.event_configs.insert(EventConfig(
        events::kChromeOpened, Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));
    config.storage_type = StorageType::DEVICE;
    return config;
  } else if (kIPHiOSPromoWhatsNewFeature.name == feature->name) {
    // Should trigger and display What's New when requested at most once a
    // month.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used = EventConfig("whats_new_promo_used", kAlwaysTrue, 365, 365);
    // What's New promo should be trigger no more than once every 14 days.
    config.trigger = EventConfig("whats_new_promo_trigger",
                                 Comparator(LESS_THAN, 1), 14, 365);
    config.event_configs.insert(EventConfig(
        events::kViewedWhatsNew, Comparator(LESS_THAN, 1), 365, 365));
    config.event_configs.insert(EventConfig(
        events::kChromeOpened, Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));

    // Only show the promo if the Welcome Back Screen hasn't been displayed
    // in the past 3 days.
    config.event_configs.insert(EventConfig(events::kIOSWelcomeBackPromoTrigger,
                                            Comparator(EQUAL, 0), 3, 365));

    return config;
  } else if (kIPHiOSPromoEphemeralThemeFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::PROFILE;
    config.used =
        EventConfig(events::kEphemeralThemePromoUsed, Comparator(EQUAL, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config.trigger =
        EventConfig("ephemeral_theme_promo_trigger", Comparator(EQUAL, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    return config;
  } else if (kIPHiOSPromoBackgroundCustomizationFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::PROFILE;
    config.used =
        EventConfig(events::kHomeBackgroundCustomizationMenuUsed,
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);
    config.trigger =
        EventConfig("background_customization_promo_trigger",
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);

    // Having a custom background can also count as interacting with the
    // feature, so no need to display the promo in that case.
    config.event_configs.insert(EventConfig(events::kNTPCustomBackgroundLoaded,
                                            Comparator(EQUAL, 0), 90, 90));

    // Also make sure that the user didn't see the older customization promos
    // recently.
    config.event_configs.insert(
        EventConfig(events::kHomeCustomizationPromoTriggered,
                    Comparator(EQUAL, 0), 30, 30));

    // An alternate trigger event was provided via Finch to re-show the IPH to
    // users when the background customization feature was being experimented
    // with.
    config.event_configs.insert(
        EventConfig("home_customization_menu_iph_triggered_2",
                    Comparator(EQUAL, 0), 30, 30));

    // Make sure the First Run Experience occurred more than 3 days ago.
    config.event_configs.insert(
        EventConfig(events::kIOSFirstRunComplete, Comparator(EQUAL, 0), 3, 3));

    return config;
  } else if (kIPHiOSPromoGenericDefaultBrowserFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    // Show this promo once every 180 days.
    config.trigger = EventConfig("generic_default_browser_promo_trigger",
                                 Comparator(EQUAL, 0), 180, kMaxStoragePeriod);

    // The off-cycle promo should count as a generic promo impression,
    // effectively putting it back on cooldown.
    config.event_configs.insert(
        EventConfig("default_browser_off_cycle_promo_trigger",
                    Comparator(EQUAL, 0), 180, kMaxStoragePeriod));

    // Show the promo if promo specific conditions are met during last 21
    // days.
    config.event_configs.insert(
        EventConfig("generic_default_browser_promo_conditions_met",
                    Comparator(GREATER_THAN, 0), 21, 365));

    return config;
  } else if (kIPHiOSPromoAllTabsFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    config.trigger = EventConfig("all_tabs_promo_trigger", Comparator(EQUAL, 0),
                                 kMaxStoragePeriod, kMaxStoragePeriod);
    config.event_configs.insert(EventConfig(
        "all_tabs_promo_conditions_met", Comparator(GREATER_THAN, 0), 21, 365));
    return config;
  } else if (kIPHiOSPromoMadeForIOSFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    config.trigger =
        EventConfig("made_for_ios_promo_trigger", Comparator(EQUAL, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config.event_configs.insert(EventConfig("made_for_ios_promo_conditions_met",
                                            Comparator(GREATER_THAN, 0), 21,
                                            365));
    return config;
  } else if (kIPHiOSPromoStaySafeFeature.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.groups.push_back(kiOSTailoredDefaultBrowserPromosGroup.name);
    config.storage_type = StorageType::DEVICE;

    config.trigger =
        EventConfig("stay_safe_promo_trigger", Comparator(EQUAL, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config.event_configs.insert(EventConfig("stay_safe_promo_conditions_met",
                                            Comparator(GREATER_THAN, 0), 21,
                                            365));
    return config;
  } else if (kIPHiOSPromoCredentialProviderExtensionFeature.name ==
             feature->name) {
    // Should show no more than 3 times. Also, the promo is first shown in a
    // different form, and then shown via this feature one day later (after
    // snoozing).
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used = EventConfig("credential_provider_extension_promo_used",
                              kAlwaysTrue, 365, 365);
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
  } else if (kIPHiOSDockingPromoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used = EventConfig("docking_promo_used", Comparator(EQUAL, 0),
                              kMaxStoragePeriod, kMaxStoragePeriod);
    // Show a maximum of 1 time per year.
    config.trigger = EventConfig("docking_promo_trigger", Comparator(EQUAL, 0),
                                 365, kMaxStoragePeriod);
    config.storage_type = StorageType::DEVICE;
    return config;
  } else if (kIPHiOSDockingPromoEligibilityFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::DEVICE;
    config.tracking_only = true;

    config.used = EventConfig("docking_promo_eligibility_used", kAlwaysTrue,
                              kMaxStoragePeriod, kMaxStoragePeriod);

    // Show this promo once in number of days specified by the feature param.
    config.trigger =
        EventConfig("docking_promo_eligibility_trigger", kAlwaysTrue,
                    kMaxStoragePeriod, kMaxStoragePeriod);

    //  L7 days active.
    config.event_configs.insert(
        EventConfig(events::kChromeActiveSessionDay, kAlwaysTrue, 7, 365));

    // L7 app icon launches.
    config.event_configs.insert(
        EventConfig(events::kIOSChromeOpenedFromIcon, kAlwaysTrue, 7, 365));

    return config;
  } else if (kIPHiOSPostDefaultAbandonmentPromoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config.used = EventConfig("post_default_abandonment_promo_used",
                              kAlwaysTrue, 365, 365);
    config.trigger =
        EventConfig("post_default_abandonment_promo_trigger",
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);
    return config;
  } else if (kIPHiOSPromoSigninFullscreenFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used = EventConfig("signin_fullscreen_promo_used", kAlwaysTrue,
                              kMaxStoragePeriod, kMaxStoragePeriod);
    config.trigger =
        EventConfig(events::kIOSSigninFullscreenPromoTrigger, kAlwaysTrue,
                    kMaxStoragePeriod, kMaxStoragePeriod);
    return config;
  } else if (kIPHiOSReaderModeLargeOmniboxEntrypointFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    // No availability requirement for this feature.
    config.availability = kAlwaysAvailable;
    // No session rate limit for this feature.
    config.session_rate = kNoRestrictions;
    config.used =
        EventConfig("reader_mode_chip_expanded_used", kAlwaysTrue, 360, 360);
    // The expanded chip should not be triggered more than 3 times per day.
    config.trigger = EventConfig(events::kIOSReaderModeChipExpanded,
                                 Comparator(LESS_THAN, 3), 1, 360);
    return config;
  } else if (kIPHiOSWelcomeBackFeature.name == feature->name) {
    // Show the promo any time the conditions are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used =
        EventConfig(events::kIOSWelcomeBackPromoUsed, Comparator(EQUAL, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config.trigger =
        EventConfig(events::kIOSWelcomeBackPromoTrigger, Comparator(EQUAL, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config.storage_type = StorageType::DEVICE;
    return config;
  } else {
    return std::nullopt;
  }
}

// Returns a config for a custom feature that does not follow the standard
// rules.
std::optional<FeatureConfig> GetCustomConfig(const base::Feature* feature) {
  if (kIPHiOSPromoPostRestoreFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used = EventConfig("post_restore_promo_used", kAlwaysTrue, 365, 365);
    // Should not be subject to impression limits, as it helps users recover
    // from being signed-out after restoring their device.
    config.trigger =
        EventConfig("post_restore_promo_trigger", kAlwaysTrue, 365, 365);
    config.storage_type = StorageType::DEVICE;
    return config;
  } else if (kIPHWhatsNewUpdatedFeature.name == feature->name) {
    // Should trigger and display What's New badged only when What's New was not
    // viewed.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    // This feature only affects badges, so shouldn't impact or block anything
    // else.
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    config.used = EventConfig("whats_new_updated_used", kAlwaysTrue, 365, 365);
    config.trigger =
        EventConfig("whats_new_updated_trigger", kAlwaysTrue, 365, 365);
    config.event_configs.insert(EventConfig(
        events::kViewedWhatsNew, Comparator(LESS_THAN, 1), 365, 365));
    config.storage_type = StorageType::DEVICE;
    return config;
  } else if (kIPHiOSPromoDefaultBrowserReminderFeature.name == feature->name) {
    // A config for a feature to handle re-showing the default browser promo
    // after a "Remind Me Later". Should trigger only if the reminder happened
    // over X days ago (i.e count == 0 in the past X days and count >= 1 in
    // general). The default configuration here allows snoozing once for 1 day,
    // but this can be changed via Finch.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.trigger = EventConfig("default_browser_promo_reminder_trigger",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig("default_browser_promo_reminder_used",
                              kAlwaysTrue, 360, 360);
    config.event_configs.insert(EventConfig(
        "default_browser_promo_remind_me_later", Comparator(EQUAL, 0), 1, 360));
    config.event_configs.insert(
        EventConfig("default_browser_promo_remind_me_later",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 360, 360));
    return config;
  } else if (kIPHiOSPromoPostRestoreDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used = EventConfig("post_restore_default_browser_promo_used",
                              kAlwaysTrue, 365, 365);
    // Should not be subject to impression limits, as it helps users recover
    // from losing default browser status after restoring their device.
    config.trigger = EventConfig("post_restore_default_browser_promo_trigger",
                                 kAlwaysTrue, 365, 365);
    return config;
  } else if (kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used =
        EventConfig("non_modal_default_browser_promo_omnibox_paste_used",
                    kAlwaysTrue, 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger =
        EventConfig(events::kNonModalDefaultBrowserPromoUrlPasteTrigger,
                    Comparator(LESS_THAN, 1), 14, 365);
    // Should be triggered no more than twice (2x) every 6 months.
    config.event_configs.insert(
        EventConfig(events::kNonModalDefaultBrowserPromoUrlPasteTrigger,
                    Comparator(LESS_THAN, 2), 180, 365));

    config.groups.push_back(kiOSTailoredNonModalDefaultBrowserPromosGroup.name);
    return config;
  } else if (kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used =
        EventConfig("non_modal_default_browser_promo_app_switcher_used",
                    kAlwaysTrue, 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger =
        EventConfig(events::kNonModalDefaultBrowserPromoAppSwitcherTrigger,
                    Comparator(LESS_THAN, 1), 14, 365);
    // Should be triggered no more than twice (2x) every 6 months.
    config.event_configs.insert(
        EventConfig(events::kNonModalDefaultBrowserPromoAppSwitcherTrigger,
                    Comparator(LESS_THAN, 2), 180, 365));

    config.groups.push_back(kiOSTailoredNonModalDefaultBrowserPromosGroup.name);
    return config;
  } else if (kIPHiOSPromoNonModalShareDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used = EventConfig("non_modal_default_browser_promo_share_used",
                              kAlwaysTrue, 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger =
        EventConfig(events::kNonModalDefaultBrowserPromoShareTrigger,
                    Comparator(LESS_THAN, 1), 14, 365);
    // Should be triggered no more than twice (2x) every 6 months.
    config.event_configs.insert(
        EventConfig(events::kNonModalDefaultBrowserPromoShareTrigger,
                    Comparator(LESS_THAN, 2), 180, 365));

    config.groups.push_back(kiOSTailoredNonModalDefaultBrowserPromosGroup.name);
    return config;
  } else if (kIPHiOSPromoNonModalSigninPasswordFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;

    config.used =
        EventConfig("non_modal_signin_promo_password", kAlwaysTrue, 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger = EventConfig(events::kNonModalSigninPromoPasswordTrigger,
                                 Comparator(LESS_THAN, 1), 14, 365);

    config.groups.push_back(kiOSNonModalSigninPromosGroup.name);
    return config;
  } else if (kIPHiOSPromoNonModalSigninBookmarkFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;

    config.used =
        EventConfig("non_modal_signin_promo_bookmark", kAlwaysTrue, 365, 365);
    // Should be triggered no more than once every 14 days.
    config.trigger = EventConfig(events::kNonModalSigninPromoBookmarkTrigger,
                                 Comparator(LESS_THAN, 1), 14, 365);

    config.groups.push_back(kiOSNonModalSigninPromosGroup.name);
    return config;
  } else if (kIPHiOSSavedTabGroupClosed.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used =
        EventConfig("saved_tab_group_closed_used", kAlwaysTrue, 365, 365);
    config.trigger = EventConfig("saved_tab_group_closed_trigger",
                                 Comparator(EQUAL, 0), 365, 365);
    return config;
  } else if (kIPHiOSSharedTabGroupForeground.name == feature->name) {
    // Should show this promo only once if promo specific and group conditions
    // are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.used =
        EventConfig("shared_tab_group_foreground_used", kAlwaysTrue, 365, 365);
    config.trigger = EventConfig("shared_tab_group_foreground_trigger",
                                 Comparator(EQUAL, 0), 365, 365);
    return config;
  } else if (kIPHiOSDefaultBrowserBannerPromoFeature.name == feature->name) {
    // Promo should show only once, and also require time since other promos.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = Comparator(EQUAL, 0);
    config.used =
        EventConfig("default_browser_banner_promo_used", kAlwaysTrue, 365, 365);
    config.trigger =
        EventConfig("default_browser_banner_promo_trigger",
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);
    // This promo counts as a default browser promo despite not being a
    // fullscreen promo from the promos manager because it's still a
    // non-contextual default browser promo. Thus, it should share cooldown
    // rules.
    config.groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    return config;
  } else if (kIPHiOSNewIAPromoFeature.name == feature->name) {
    // Promo should show only once.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = Comparator(EQUAL, 0);
    config.used = EventConfig("new_ia_promo_used", kAlwaysTrue, 365, 365);
    config.trigger = EventConfig("new_ia_promo_trigger", Comparator(EQUAL, 0),
                                 kMaxStoragePeriod, kMaxStoragePeriod);
    return config;
  } else if (kIPHiOSDefaultBrowserOffCyclePromoFeature.name == feature->name) {
    // A config for a feature to handle the off-cycle generic default browser
    // promo.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = kAlwaysAvailable;
    config->session_rate = kNoRestrictions;
    config->groups.push_back(kiOSDefaultBrowserPromosGroup.name);
    config->storage_type = StorageType::DEVICE;
    config->used = EventConfig("default_browser_off_cycle_promo_used",
                               kAlwaysTrue, 365, 365);

    config->trigger = EventConfig(
        "default_browser_off_cycle_promo_trigger", Comparator(EQUAL, 0),
        kIPHiOSDefaultBrowserOffCyclePromoCooldown.Get(), kMaxStoragePeriod);
    return config;
  } else if (kIPHiOSSafariImportFeature.name == feature->name) {
    // A config that shows the Safari import entry point modal. If the user
    // proceeds with the import or dismisses the modal, the entry point will
    // show again.
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    // Can be triggered any number of times, as long as the user keeps setting
    // the reminder.
    config.trigger =
        EventConfig("ios_safari_import_entry_point_trigger", kAlwaysTrue,
                    kMaxStoragePeriod, kMaxStoragePeriod);
    // If the user has started or dismissed the Safari import workflow, don't
    // show the entry point again.
    config.used =
        EventConfig("ios_safari_import_entry_point_used_or_dismissed",
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);
    // Show the entry point if the user has neither started or dismissed the
    // Safari import workflow, nor tapped "remind me later" in the last two
    // days.
    config.event_configs.insert(EventConfig(
        events::kIOSSafariImportRemindMeLater, Comparator(EQUAL, 0), 2, 2));
    config.storage_type = StorageType::DEVICE;
    return config;
  } else if (kIPHiOSOneTimeDefaultBrowserNotificationFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::DEVICE;
    config.trigger =
        EventConfig("one_time_default_browser_notification_trigger",
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);
    config.event_configs.insert(
        EventConfig(events::kDefaultBrowserPromosGroupTrigger,
                    Comparator(EQUAL, 0), 14, 360));
    config.event_configs.insert(EventConfig(events::kIOSDefaultBrowserFREShown,
                                            Comparator(EQUAL, 0), 7, 365));
    return config;
  } else if (kIPHiOSActiveDaysTrackingFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::DEVICE;
    config.tracking_only = true;

    config.used = EventConfig("active_days_tracking_used", kAlwaysTrue,
                              kMaxStoragePeriod, kMaxStoragePeriod);

    config.trigger = EventConfig("active_days_tracking_trigger", kAlwaysTrue,
                                 kMaxStoragePeriod, kMaxStoragePeriod);

    config.event_configs.insert(EventConfig(events::kChromeActiveSessionDay,
                                            kAlwaysTrue, 8, kMaxStoragePeriod));
    config.event_configs.insert(EventConfig(
        events::kChromeActiveSessionDay, kAlwaysTrue, 15, kMaxStoragePeriod));
    config.event_configs.insert(EventConfig(
        events::kChromeActiveSessionDay, kAlwaysTrue, 29, kMaxStoragePeriod));
    return config;
  } else if (kIPHiOSBackendPromoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::PROFILE;

    // Backend promos are configured and capped from the backend. Here we only
    // want to control the cooldowns between backend promos and other promos
    // that are part of the kiOSFullscreenPromosGroup group.
    // Being part of kiOSFullscreenPromosGroup will allow this promo only if
    // there was no other promo from this group in last 2 days and less than 3
    // in the last 7 days.
    config.groups.push_back(kiOSFullscreenPromosGroup.name);
    config.used = EventConfig("ios_backend_promo_used", kAlwaysTrue,
                              kMaxStoragePeriod, kMaxStoragePeriod);
    config.trigger = EventConfig("ios_backend_promo_trigger", kAlwaysTrue,
                                 kMaxStoragePeriod, kMaxStoragePeriod);
    return config;
  } else if (kIPHiOSPromoSettingsCardDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::DEVICE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    // Promo cannot be shown again once dismissed.
    config.used = EventConfig(events::kDefaultBrowserSettingsCardPromoUsed,
                              Comparator(LESS_THAN, 1), kMaxStoragePeriod,
                              kMaxStoragePeriod);

    // Show this promo once every 7 days.
    config.trigger =
        EventConfig("default_browser_settings_card_promo_trigger",
                    Comparator(LESS_THAN, 1), 7, kMaxStoragePeriod);

    // Promo card should only be shown 4 times max.
    config.event_configs.insert(EventConfig(
        "default_browser_settings_card_promo_trigger", Comparator(LESS_THAN, 4),
        kMaxStoragePeriod, kMaxStoragePeriod));

    // Default Browser promos should be shown after 3 or more days since FRE.
    config.event_configs.insert(EventConfig(events::kIOSDefaultBrowserFREShown,
                                            Comparator(EQUAL, 0), 3, 365));

    // Default Browser promos can be shown only after Chrome has been opened 7
    // or more times.
    config.event_configs.insert(EventConfig(
        events::kChromeOpened, Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));

    // Default Browser promos shouldn't be shown if the Post Restore Default
    // Browser Promo has been shown in the past 7 days.
    config.event_configs.insert(
        EventConfig("post_restore_default_browser_promo_trigger",
                    Comparator(EQUAL, 0), 7, 365));

    return config;
  } else if (kIPHiOSPromoSettingsCellDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::DEVICE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    config.used = EventConfig(events::kDefaultBrowserSettingsCellPromoUsed,
                              Comparator(LESS_THAN, 1), kMaxStoragePeriod,
                              kMaxStoragePeriod);

    // No impression limit.
    config.trigger =
        EventConfig("default_browser_settings_cell_promo_trigger", kAlwaysTrue,
                    kMaxStoragePeriod, kMaxStoragePeriod);

    // Default Browser promos should be shown after 3 or more days since FRE.
    config.event_configs.insert(EventConfig(events::kIOSDefaultBrowserFREShown,
                                            Comparator(EQUAL, 0), 3, 365));

    // Default Browser promos can be shown only after Chrome has been opened 7
    // or more times.
    config.event_configs.insert(EventConfig(
        events::kChromeOpened, Comparator(GREATER_THAN_OR_EQUAL, 7), 365, 365));

    // Default Browser promos shouldn't be shown if the Post Restore Default
    // Browser Promo has been shown in the past 7 days.
    config.event_configs.insert(
        EventConfig("post_restore_default_browser_promo_trigger",
                    Comparator(EQUAL, 0), 7, 365));

    return config;
  } else if (kIPHiOSPromoOverflowMenuDestinationDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::DEVICE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    config.used =
        EventConfig(events::kDefaultBrowserPromoOverflowMenuDestinationUsed,
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);

    config.trigger =
        EventConfig("default_browser_promo_overflow_menu_destination_trigger",
                    kAlwaysTrue, kMaxStoragePeriod, kMaxStoragePeriod);

    return config;
  } else if (kIPHiOSPromoOverflowMenuShortcutsDefaultBrowserFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;
    config.storage_type = StorageType::DEVICE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    config.used =
        EventConfig(events::kDefaultBrowserPromoOverflowMenuShortcutsUsed,
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);

    config.trigger =
        EventConfig("default_browser_promo_overflow_menu_shortcuts_trigger",
                    kAlwaysTrue, kMaxStoragePeriod, kMaxStoragePeriod);

    return config;
  } else if (kIPHiOSPromoContextualDefaultBrowserGeminiFeature.name ==
             feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysAvailable;
    config.session_rate = kNoRestrictions;

    // Time between impressions: 14 days.
    config.trigger =
        EventConfig("contextual_default_browser_promo_gemini_trigger",
                    Comparator(LESS_THAN, 1), 14, kMaxStoragePeriod);

    // Max Impression cap: 4.
    config.event_configs.insert(EventConfig(
        "contextual_default_browser_promo_gemini_trigger",
        Comparator(LESS_THAN, 4), kMaxStoragePeriod, kMaxStoragePeriod));

    // Precondition: Gemini session must have been terminated within the last
    // 14 days.
    config.event_configs.insert(EventConfig(events::kGeminiSessionTerminated,
                                            Comparator(GREATER_THAN, 0), 14,
                                            kMaxStoragePeriod));

    config.used =
        EventConfig(events::kDefaultBrowserPromoContextualGeminiUsed,
                    Comparator(EQUAL, 0), kMaxStoragePeriod, kMaxStoragePeriod);

    return config;
  } else if (kIPHiOSPromoLevelUpFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = kAlwaysTrue;
    // Feature can only be used once.
    config.used =
        EventConfig(events::kIOSLevelUpPromoUsed, Comparator(LESS_THAN, 1),
                    kMaxStoragePeriod, kMaxStoragePeriod);

    // Max Impression cap: 1.
    config.trigger =
        EventConfig("level_up_promo_trigger", Comparator(LESS_THAN, 1),
                    kMaxStoragePeriod, kMaxStoragePeriod);

    // Available 4 days after new user completes first run.
    config.event_configs.insert(EventConfig(events::kIOSFirstRunComplete,
                                            Comparator(LESS_THAN, 1), 4,
                                            kMaxStoragePeriod));
    return config;
  } else {
    return std::nullopt;
  }
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
