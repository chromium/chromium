// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_configurations.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"

#if BUILDFLAG(IS_IOS)
#include "components/feature_engagement/public/ios_promo_feature_configuration.h"
#endif  // BUILDFLAG(IS_IOS)

namespace {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const int k10YearsInDays = 365 * 10;
#endif
}  // namespace

namespace feature_engagement {

FeatureConfig CreateAlwaysTriggerConfig(const base::Feature* feature) {
  // Trim "IPH_" prefix from the feature name to use for trigger and used
  // events.
  const char* prefix = "IPH_";
  std::string stripped_feature_name = feature->name;
  if (base::StartsWith(stripped_feature_name, prefix,
                       base::CompareCase::SENSITIVE))
    stripped_feature_name = stripped_feature_name.substr(strlen(prefix));

  // A config that always meets condition to trigger IPH.
  FeatureConfig config;
  config.valid = true;
  config.availability = Comparator(ANY, 0);
  config.session_rate = Comparator(ANY, 0);
  config.trigger = EventConfig(stripped_feature_name + "_trigger",
                               Comparator(ANY, 0), 90, 90);
  config.used =
      EventConfig(stripped_feature_name + "_used", Comparator(ANY, 0), 90, 90);
  return config;
}

absl::optional<FeatureConfig> GetClientSideFeatureConfig(
    const base::Feature* feature) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (kIPHPasswordsAccountStorageFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHPasswordsManagementBubbleAfterSaveFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->trigger =
        EventConfig("password_saved", Comparator(LESS_THAN, 1), 180, 180);
    config->session_rate = Comparator(ANY, 0);
    config->availability = Comparator(ANY, 0);
    return config;
  }

  if (kIPHPasswordsManagementBubbleDuringSigninFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->trigger =
        EventConfig("signin_flow_detected", Comparator(LESS_THAN, 1), 180, 180);
    config->session_rate = Comparator(ANY, 0);
    config->availability = Comparator(ANY, 0);
    return config;
  }

  if (kIPHProfileSwitchFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the profile menu was not opened.
    config->trigger =
        EventConfig("profile_switch_trigger", Comparator(EQUAL, 0), 360, 360);
    config->used =
        EventConfig("profile_menu_shown", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHReadingListInSidePanelFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the side panel was not opened.
    config->trigger =
        EventConfig("side_panel_trigger", Comparator(EQUAL, 0), 360, 360);
    config->used =
        EventConfig("side_panel_shown", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHReadingModeSidePanelFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // Show the promo up to 3 times a year.
    config->trigger = EventConfig("iph_reading_mode_side_panel_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used = EventConfig("reading_mode_side_panel_shown",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHGMCCastStartStopFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("gmc_start_stop_iph_trigger",
                                  Comparator(EQUAL, 0), 180, 180);
    config->used = EventConfig("media_route_stopped_from_gmc",
                               Comparator(EQUAL, 0), 180, 180);
    return config;
  }

  if (kIPHDesktopSharedHighlightingFeature.name == feature->name) {
    // A config that allows the shared highlighting desktop IPH to be shown
    // when a user receives a highlight:
    // * Once per 7 days
    // * Up to 5 times but only if unused in the last 7 days.
    // * Used fewer than 2 times

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("iph_desktop_shared_highlighting_trigger",
                                  Comparator(LESS_THAN, 5), 360, 360);
    config->used = EventConfig("iph_desktop_shared_highlighting_used",
                               Comparator(LESS_THAN, 2), 360, 360);
    config->event_configs.insert(
        EventConfig("iph_desktop_shared_highlighting_trigger",
                    Comparator(EQUAL, 0), 7, 360));
    return config;
  }

  if (kIPHCookieControlsFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // Show promo up to 3 times per year and only if user hasn't interacted with
    // the cookie controls bubble in the last week.
    config->trigger = EventConfig("iph_cookie_controls_triggered",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used =
        EventConfig(feature_engagement::events::kCookieControlsBubbleShown,
                    Comparator(EQUAL, 0), 7, 7);
    return config;
  }

  if (kIPHBatterySaverModeFeature.name == feature->name) {
    // Show promo once a year when the battery saver toolbar icon is visible.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("battery_saver_info_triggered",
                                  Comparator(LESS_THAN, 1), 360, 360);
    config->used =
        EventConfig("battery_saver_info_shown", Comparator(EQUAL, 0), 7, 360);
    return config;
  }

  if (kIPHHighEfficiencyModeFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // Show the promo max 3 times, once per week.
    config->trigger = EventConfig("high_efficiency_prompt_in_trigger",
                                  Comparator(LESS_THAN, 1), 7, 360);
    // This event is never logged but is included for consistency.
    config->used = EventConfig("high_efficiency_prompt_in_used",
                               Comparator(EQUAL, 0), 360, 360);
    config->event_configs.insert(
        EventConfig("high_efficiency_prompt_in_trigger",
                    Comparator(LESS_THAN, 1), 360, 360));
    return config;
  }

  if (kIPHPerformanceNewBadgeFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the new badge max 20 times within a year
    config->trigger = EventConfig("performance_new_badge_shown",
                                  Comparator(LESS_THAN, 20), 360, 360);

    // Badge stops showing after the user uses it 3 times
    config->used = EventConfig("performance_activated",
                               Comparator(LESS_THAN, 3), 360, 360);
    return config;
  }

  if (kIPHPowerBookmarksSidePanelFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("iph_power_bookmarks_side_panel_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used = EventConfig("power_bookmarks_side_panel_shown",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHPriceInsightsPageActionIconLabelFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the label once per day, 3 times max in 28 days.
    config->trigger =
        EventConfig("price_insights_page_action_icon_label_in_trigger",
                    Comparator(LESS_THAN, 1), 1, 360);
    config->used = EventConfig("price_insights_page_action_icon_label_used",
                               Comparator(EQUAL, 0), 28, 360);
    config->event_configs.insert(
        EventConfig("price_insights_page_action_icon_label_in_trigger",
                    Comparator(LESS_THAN, 3), 28, 360));
    return config;
  }

  if (kIPHPriceTrackingChipFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the promo only once.
    config->trigger =
        EventConfig("price_tracking_chip_iph_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // Set a dummy config for the used event to be consistent with the other
    // IPH configurations. The used event is never recorded by the feature code
    // because the trigger event is already reported the first time the chip is
    // being used, which corresponds to a used event.
    config->used =
        EventConfig("price_tracking_chip_shown", Comparator(ANY, 0), 0, 360);
    return config;
  }

  if (kIPHPriceTrackingEmailConsentFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the IPH up to 3 times per month.
    config->trigger = EventConfig("price_tracking_email_consent_trigger",
                                  Comparator(LESS_THAN, 3), 30, 360);
    return config;
  }

  if (kIPHPriceTrackingInSidePanelFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the price tracking IPH was not triggered.
    config->trigger = EventConfig("iph_price_tracking_side_panel_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("price_tracking_side_panel_shown",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHPriceTrackingPageActionIconLabelFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the promo once per day.
    config->trigger =
        EventConfig("price_tracking_page_action_icon_label_in_trigger",
                    Comparator(LESS_THAN, 1), 1, 360);
    return config;
  }

  if (kIPHShoppingCollectionFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the IPH 3 times per year.
    config->trigger = EventConfig("shopping_collection_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    return config;
  }

  if (kIPHExtensionsMenuFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);

    // Show promo up to three times a year or until the extensions menu is
    // opened while any extension has access to the current site.
    config->trigger = EventConfig("extensions_menu_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used =
        EventConfig("extensions_menu_opened_while_extension_has_access",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHExtensionsRequestAccessButtonFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);

    // Show promo up to three times a year or until the request access button
    // is clicked.
    config->trigger = EventConfig("extensions_request_access_button_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used = EventConfig("extensions_request_access_button_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHCompanionSidePanelFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // Show the promo up to 3 times a year.
    config->trigger = EventConfig("iph_companion_side_panel_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used =
        EventConfig("companion_side_panel_accessed_via_toolbar_button",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHCompanionSidePanelRegionSearchFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // Show the promo up to 3 times a year.
    config->trigger =
        EventConfig("iph_companion_side_panel_region_search_trigger",
                    Comparator(LESS_THAN, 3), 360, 360);
    config->used =
        EventConfig("companion_side_panel_region_search_button_clicked",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHDesktopCustomizeChromeFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Used to increase the usage of Customize Chrome for users who have opened
    // it 0 times in the last 360 days.
    config->used =
        EventConfig("customize_chrome_opened", Comparator(EQUAL, 0), 360, 360);
    // Triggered when IPH hasn't been shown in the past day.
    config->trigger = EventConfig("iph_customize_chrome_triggered",
                                  Comparator(EQUAL, 0), 1, 360);
    config->snooze_params.max_limit = 4;
    return config;
  }

  if (kIPHDesktopCustomizeChromeRefreshFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show IPH regardless of customize_chrome usage
    config->used =
        EventConfig("customize_chrome_opened", Comparator(ANY, 0), 360, 360);
    // Triggered when IPH has been shown less than twice this year.
    config->trigger = EventConfig("iph_customize_chrome_refresh_triggered",
                                  Comparator(LESS_THAN, 2), 360, 360);
    config->snooze_params.max_limit = 4;
    return config;
  }

  if (kIPHPasswordsWebAppProfileSwitchFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("iph_passwords_web_app_profile_switch_triggered",
                    Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("web_app_profile_menu_shown",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHPasswordManagerShortcutFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("iph_password_manager_shortcut_triggered",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("password_manager_shortcut_created",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHDownloadToolbarButtonFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;
    // Show the promo max once a year if the user hasn't interacted with the
    // download bubble within the last 21 days.
    config->trigger = EventConfig("download_bubble_iph_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("download_bubble_interaction",
                               Comparator(EQUAL, 0), 21, 360);
    return config;
  }

  if (kIPHBackNavigationMenuFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("back_navigation_menu_iph_is_triggered",
                                  Comparator(LESS_THAN_OR_EQUAL, 4), 360, 360);
    config->used = EventConfig("back_navigation_menu_is_opened",
                               Comparator(EQUAL, 0), 7, 360);
    config->snooze_params.snooze_interval = 7;
    config->snooze_params.max_limit = 4;
    return config;
  }

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (kIPHiOSPasswordPromoDesktopFeature.name == feature->name) {
    // A config for allowing other IPH's to explicitly block the iOS password
    // promo bubble on desktop if needed. By default it is non-blocking and
    // non-blocked by other IPH due it being highly contextual, but this FET
    // config and feature exist to allow some FET control over this promo if
    // needed.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::ALL;
    config->blocking.type = Blocking::Type::ALL;
    config->used =
        EventConfig("ios_password_promo_bubble_on_desktop_interacted_with",
                    Comparator(ANY, 0), 0, 0);
    config->trigger = EventConfig("ios_password_promo_bubble_on_desktop_shown",
                                  Comparator(ANY, 0), 0, 0);
    return config;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_ANDROID)

  if (kIPHDataSaverDetailFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
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
    absl::optional<FeatureConfig> config = FeatureConfig();
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
    absl::optional<FeatureConfig> config = FeatureConfig();
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
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("download_home_iph_trigger", Comparator(EQUAL, 0), 90, 360);
    config->used =
        EventConfig("download_home_opened", Comparator(EQUAL, 0), 90, 360);
    config->event_configs.insert(EventConfig(
        "download_completed", Comparator(GREATER_THAN_OR_EQUAL, 1), 90, 360));
    config->snooze_params.snooze_interval = 7;
    config->snooze_params.max_limit = 3;
    return config;
  }
  if (kIPHDownloadIndicatorFeature.name == feature->name) {
    // A config that allows the DownloadIndicator IPH to be shown up to 2 times,
    // but only if download home hasn't been opened in the last 90 days.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("download_indicator_iph_trigger",
                                  Comparator(LESS_THAN, 2), 360, 360);
    config->used =
        EventConfig("download_home_opened", Comparator(EQUAL, 0), 90, 360);
    return config;
  }
  if (kIPHContextualPageActionsQuietVariantFeature.name == feature->name) {
    // A config that allows the contextual page action IPH to be shown:
    // * Once per day. 3 times max in 90 days
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("contextual_page_actions_quiet_variant_iph_trigger",
                    Comparator(LESS_THAN, 1), 1, 360);
    config->used = EventConfig("contextual_page_actions_quiet_variant_used",
                               Comparator(EQUAL, 0), 90, 360);
    config->event_configs.insert(
        EventConfig("contextual_page_actions_quiet_variant_iph_trigger",
                    Comparator(LESS_THAN, 3), 90, 360));
    return config;
  }
  if (kIPHContextualPageActionsActionChipFeature.name == feature->name) {
    // A config that allows the Contextual Page Action Chip to be shown:
    // * 3 times per session.
    // * 5 times per day.
    // * 10 times per week.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 3);
    config->trigger =
        EventConfig("contextual_page_actions_action_chip_iph_trigger",
                    Comparator(LESS_THAN, 5), 1, 360);
    config->event_configs.insert(
        EventConfig("contextual_page_actions_action_chip_iph_trigger",
                    Comparator(LESS_THAN, 10), 7, 360));
    return config;
  }
  if (kIPHAddToHomescreenMessageFeature.name == feature->name) {
    // A config that allows the Add to homescreen message IPH to be shown:
    // * Once per 15 days
    // * Up to 2 times but only if unused in the last 15 days.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("add_to_homescreen_message_iph_trigger",
                                  Comparator(LESS_THAN, 2), 90, 90);
    config->used = EventConfig("add_to_homescreen_dialog_shown",
                               Comparator(EQUAL, 0), 90, 90);
    config->event_configs.insert(EventConfig(
        "add_to_homescreen_message_iph_trigger", Comparator(EQUAL, 0), 15, 90));
    return config;
  }

  // Feature notification guide help UI promos that are shown in response to a
  // notification click.
  if (kIPHFeatureNotificationGuideDefaultBrowserPromoFeature.name ==
          feature->name ||
      kIPHFeatureNotificationGuideSignInHelpBubbleFeature.name ==
          feature->name ||
      kIPHFeatureNotificationGuideIncognitoTabHelpBubbleFeature.name ==
          feature->name ||
      kIPHFeatureNotificationGuideNTPSuggestionCardHelpBubbleFeature.name ==
          feature->name ||
      kIPHFeatureNotificationGuideVoiceSearchHelpBubbleFeature.name ==
          feature->name) {
    return CreateAlwaysTriggerConfig(feature);
  }

  // A generic feature that always returns true.
  if (kIPHGenericAlwaysTriggerHelpUiFeature.name == feature->name) {
    return CreateAlwaysTriggerConfig(feature);
  }

  if (kIPHFeatureNotificationGuideIncognitoTabUsedFeature.name ==
      feature->name) {
    // A config that allows to check whether use has used incognito tabs before.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    // unused.
    config->trigger =
        EventConfig("feature_notification_guide_dummy_feature_trigger",
                    Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("feature_notification_guide_dummy_feature_used",
                               Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("app_menu_new_incognito_tab_clicked",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 90, 90));
    return config;
  }

  if (kIPHFeatureNotificationGuideVoiceSearchUsedFeature.name ==
      feature->name) {
    // A config that allows to check whether use has used voice search from NTP
    // before.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    // unused.
    config->trigger =
        EventConfig("feature_notification_guide_dummy_feature_trigger",
                    Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("feature_notification_guide_dummy_feature_used",
                               Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("ntp_voice_search_button_clicked",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 90, 90));
    return config;
  }

  if (kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature.name ==
      feature->name) {
    // A config that allows the feature guide incognito tab notification to be
    // shown.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    config->trigger = EventConfig(
        "feature_notification_guide_incognito_tab_notification_trigger",
        Comparator(LESS_THAN, 1), 90, 90);
    config->used = EventConfig("app_menu_new_incognito_tab_clicked",
                               Comparator(EQUAL, 0), 90, 90);
    return config;
  }

  if (kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature
          .name == feature->name) {
    // A config that allows the feature guide NTP suggestions cards notification
    // to be shown.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    SessionRateImpact session_rate_impact;
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    config->trigger = EventConfig(
        "feature_notification_guide_ntp_suggestion_card_notification_trigger",
        Comparator(LESS_THAN, 1), 90, 90);
    config->used = EventConfig(
        "feature_notification_guide_ntp_suggestion_card_notification_used",
        Comparator(EQUAL, 0), 90, 90);
    return config;
  }

  if (kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature.name ==
      feature->name) {
    // A config that allows the feature guide voice search notification to be
    // shown.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    SessionRateImpact session_rate_impact;
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    config->trigger = EventConfig(
        "feature_notification_guide_voice_search_notification_trigger",
        Comparator(LESS_THAN, 1), 90, 90);
    config->used = EventConfig("ntp_voice_search_button_clicked",
                               Comparator(EQUAL, 0), 90, 90);
    return config;
  }

  if (kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature.name ==
      feature->name) {
    // A config that allows the feature guide default browser notification to be
    // shown.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    SessionRateImpact session_rate_impact;
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    config->trigger = EventConfig(
        "feature_notification_guide_default_browser_notification_trigger",
        Comparator(LESS_THAN, 1), 90, 90);
    config->used = EventConfig(
        "feature_notification_guide_default_browser_notification_used",
        Comparator(EQUAL, 0), 90, 90);
    return config;
  }

  if (kIPHFeatureNotificationGuideSignInNotificationShownFeature.name ==
      feature->name) {
    // A config that allows the feature guide sign-in notification to be
    // shown.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    SessionRateImpact session_rate_impact;
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    config->trigger =
        EventConfig("feature_notification_guide_sign_in_notification_trigger",
                    Comparator(LESS_THAN, 1), 90, 90);
    config->used =
        EventConfig("feature_notification_sign_in_search_notification_used",
                    Comparator(EQUAL, 0), 90, 90);
    return config;
  }

  if (kIPHLowUserEngagementDetectorFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    config->trigger = EventConfig("low_user_engagement_detector_trigger",
                                  Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("low_user_engagement_detector_used",
                               Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(EventConfig("foreground_session_destroyed",
                                             Comparator(LESS_THAN_OR_EQUAL, 3),
                                             14, 14));
    return config;
  }

  if (kIPHFeedHeaderMenuFeature.name == feature->name) {
    // A config that allows the feed header menu IPH to be shown only once when
    // the user starts using a version of the feed that uploads click and view
    // actions.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);

    config->session_rate = Comparator(ANY, 0);
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;

    // Keep the IPH trigger event for 10 years, which is a relatively long time
    // period that we could consider as being "forever".
    config->trigger =
        EventConfig("iph_feed_header_menu_triggered", Comparator(LESS_THAN, 1),
                    k10YearsInDays, k10YearsInDays);
    // Set a dummy config for the used event to be consistent with the other
    // IPH configurations. The used event is never recorded by the feature code
    // because the trigger event is already reported the first time the feed is
    // being used, which corresponds to a used event.
    config->used =
        EventConfig("iph_feed_header_menu_used", Comparator(EQUAL, 0),
                    k10YearsInDays, k10YearsInDays);
    return config;
  }

  if (kIPHWebFeedAwarenessFeature.name == feature->name) {
    // A config that allows the web feed IPH to be shown up to three times
    // total, no more than once per session.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);

    config->session_rate = Comparator(LESS_THAN, 1);
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::ALL;
    config->session_rate_impact = session_rate_impact;

    // Keep the IPH trigger event for 10 years, which is a relatively long time
    // period that we could consider as being "forever".
    config->trigger =
        EventConfig("iph_web_feed_awareness_triggered",
                    Comparator(LESS_THAN, 3), k10YearsInDays, k10YearsInDays);
    config->used = EventConfig("web_feed_awareness_used", Comparator(ANY, 0),
                               k10YearsInDays, k10YearsInDays);
    return config;
  }

  if (kIPHFeedSwipeRefresh.name == feature->name) {
    // A config that allows the feed swipe refresh message IPH to be shown:
    // * Once per 15 days
    // * Up to 2 times but only if unused in the last 15 days.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("feed_swipe_refresh_iph_trigger",
                                  Comparator(LESS_THAN, 2), 90, 90);
    config->used =
        EventConfig("feed_swipe_refresh_shown", Comparator(EQUAL, 0), 90, 90);
    config->event_configs.insert(EventConfig("feed_swipe_refresh_iph_trigger",
                                             Comparator(EQUAL, 0), 15, 90));
    return config;
  }
  if (kIPHShoppingListMenuItemFeature.name == feature->name) {
    // Allows a shopping list menu item IPH to be displayed at most:
    // * Once per week.
    // * Up to 3 times per year.
    // * And only as long as the user has never initiated price tracking from
    // the menu.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 1);
    config->trigger = EventConfig("shopping_list_menu_item_iph_triggered",
                                  Comparator(EQUAL, 0), 7, 7);
    config->event_configs.insert(
        EventConfig("shopping_list_menu_item_iph_triggered",
                    Comparator(LESS_THAN, 3), 360, 360));
    config->used = EventConfig("shopping_list_track_price_from_menu",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHTabSwitcherButtonFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger =
        EventConfig("tab_switcher_iph_triggered", Comparator(EQUAL, 0), 90, 90);
    config->used = EventConfig("tab_switcher_button_clicked",
                               Comparator(EQUAL, 0), 14, 90);
    config->snooze_params.snooze_interval = 7;
    config->snooze_params.max_limit = 3;
    return config;
  }
  if (kIPHWebFeedFollowFeature.name == feature->name) {
    // A config that allows the WebFeed follow intro to be shown up to 5x per
    // week.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("web_feed_follow_intro_trigger",
                                  Comparator(LESS_THAN, 5), 7, 360);
    config->used = EventConfig("web_feed_follow_intro_clicked",
                               Comparator(ANY, 0), 360, 360);
    return config;
  }

  if (kIPHWebFeedPostFollowDialogFeature.name == feature->name) {
    // A config that allows one of the WebFeed post follow dialogs to be
    // presented 3 times.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("web_feed_post_follow_dialog_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used = EventConfig("web_feed_post_follow_dialog_shown",
                               Comparator(ANY, 0), 360, 360);
    return config;
  }

  if (kIPHVideoTutorialNTPChromeIntroFeature.name == feature->name) {
    // A config that allows the chrome intro video tutorial card to show up
    // until explicitly interacted upon.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("tutorial_chrome_intro_iph_trigger",
                                  Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("chrome_intro", Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("video_tutorial_iph_clicked_chrome_intro",
                    Comparator(EQUAL, 0), 90, 90));
    config->event_configs.insert(
        EventConfig("video_tutorial_iph_dismissed_chrome_intro",
                    Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPDownloadFeature.name == feature->name) {
    // A config that allows the download video tutorial card to show up
    // until explicitly interacted upon.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("tutorial_download_iph_trigger",
                                  Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("download", Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(EventConfig(
        "video_tutorial_iph_clicked_download", Comparator(EQUAL, 0), 90, 90));
    config->event_configs.insert(EventConfig(
        "video_tutorial_iph_dismissed_download", Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPSearchFeature.name == feature->name) {
    // A config that allows the search video tutorial card to show up
    // until explicitly interacted upon.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("tutorial_search_iph_trigger", Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("search", Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(EventConfig(
        "video_tutorial_iph_clicked_search", Comparator(EQUAL, 0), 90, 90));
    config->event_configs.insert(EventConfig(
        "video_tutorial_iph_dismissed_search", Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPVoiceSearchFeature.name == feature->name) {
    // A config that allows the voice search video tutorial card to show up
    // until explicitly interacted upon.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("tutorial_voice_search_iph_trigger",
                                  Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("voice_search", Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("video_tutorial_iph_clicked_voice_search",
                    Comparator(EQUAL, 0), 90, 90));
    config->event_configs.insert(
        EventConfig("video_tutorial_iph_dismissed_voice_search",
                    Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPSummaryFeature.name == feature->name) {
    // A config that allows the summary video tutorial card to show up
    // until explicitly interacted upon.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("tutorial_summary_iph_trigger", Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("summary", Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(EventConfig(
        "video_tutorial_iph_dismissed_summary", Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialTryNowFeature.name == feature->name) {
    // A config that allows the video tutorials Try Now button click to result
    // in an IPH bubble. This IPH is shown always regardless of session rate or
    // any other conditions.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("tutorial_try_now_iph_trigger", Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("try_now", Comparator(ANY, 0), 90, 90);

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;

    return config;
  }

  if (kIPHSharedHighlightingReceiverFeature.name == feature->name) {
    // A config that allows the shared highlighting message IPH to be shown
    // when a user receives a highlight:
    // * Once per 7 days
    // * Up to 5 times but only if unused in the last 7 days.
    // * Used fewer than 2 times

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("iph_shared_highlighting_receiver_trigger",
                                  Comparator(LESS_THAN, 5), 360, 360);
    config->used = EventConfig("iph_shared_highlighting_used",
                               Comparator(LESS_THAN, 2), 360, 360);
    config->event_configs.insert(
        EventConfig("iph_shared_highlighting_receiver_trigger",
                    Comparator(EQUAL, 0), 7, 360));
    return config;
  }

  if (kIPHSharingHubWebnotesStylizeFeature.name == feature->name) {
    // A config that allows the Webnotes Stylize IPH to be shown up to 6 times,
    // but only if the feature home hasn't been used in the last 360 days.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("sharing_hub_webnotes_stylize_iph_trigger",
                                  Comparator(LESS_THAN, 6), 360, 360);
    config->used = EventConfig("sharing_hub_webnotes_stylize_used",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHAutoDarkOptOutFeature.name == feature->name) {
    // A config that allows the auto dark dialog to be shown when a user
    // disables the feature for a site in the app menu when:
    // * They have not yet opened auto dark settings.
    // * The dialog has been shown 0 times before.
    // * They have done so at least 3 times.
    // TODO(crbug.com/1251737): Update this config from test values; Will
    // likely depend on giving feedback instead of opening settings, since the
    // primary purpose  of the dialog has changed.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used =
        EventConfig("auto_dark_settings_opened", Comparator(EQUAL, 0), 90, 90);
    config->trigger = EventConfig("auto_dark_opt_out_iph_trigger",
                                  Comparator(EQUAL, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("auto_dark_disabled_in_app_menu",
                    Comparator(GREATER_THAN_OR_EQUAL, 3), 90, 90));
    return config;
  }

  if (kIPHAutoDarkUserEducationMessageFeature.name == feature->name) {
    // A config that allows the auto dark message to be shown:
    // * Until the user opens auto dark settings
    // * 2 times per week
    // * Up to 6 times (3 weeks)
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("auto_dark_settings_opened",
                               Comparator(EQUAL, 0), 360, 360);
    config->trigger = EventConfig("auto_dark_user_education_message_trigger",
                                  Comparator(LESS_THAN, 2), 7, 360);
    config->event_configs.insert(
        EventConfig("auto_dark_user_education_message_trigger",
                    Comparator(LESS_THAN, 6), 360, 360));
    return config;
  }

  if (kIPHAutoDarkUserEducationMessageOptInFeature.name == feature->name) {
    // A config that allows the auto dark message to be shown:
    // * Until the user opens auto dark settings
    // * 2 times per week
    // * Up to 6 times (3 weeks)
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("auto_dark_settings_opened",
                               Comparator(EQUAL, 0), 360, 360);
    config->trigger =
        EventConfig("auto_dark_user_education_message_opt_in_trigger",
                    Comparator(LESS_THAN, 2), 7, 360);
    config->event_configs.insert(
        EventConfig("auto_dark_user_education_message_opt_in_trigger",
                    Comparator(LESS_THAN, 6), 360, 360));
    return config;
  }

  if (kIPHInstanceSwitcherFeature.name == feature->name) {
    // A config that allows the 'Manage windows' text bubble IPH to be shown
    // only once when the user starts using the multi-instance feature by
    // opening more than one window.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("instance_switcher_iph_trigger", Comparator(LESS_THAN, 1),
                    k10YearsInDays, k10YearsInDays);
    config->used = EventConfig("instance_switcher_used", Comparator(EQUAL, 0),
                               k10YearsInDays, k10YearsInDays);
    return config;
  }

  if (kIPHReadLaterAppMenuBookmarkThisPageFeature.name == feature->name) {
    // A config that allows the reading list IPH bubble to prompt the user to
    // bookmark this page.
    // This will only occur once every 60 days.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("read_later_app_menu_bookmark_this_page_iph_trigger",
                    Comparator(EQUAL, 0), 60, 60);
    config->used = EventConfig("app_menu_bookmark_star_icon_pressed",
                               Comparator(EQUAL, 0), 60, 60);

    return config;
  }

  if (kIPHReadLaterAppMenuBookmarksFeature.name == feature->name) {
    // A config that allows the reading list IPH bubble to promopt the user to
    // open the reading list.
    // This will only occur once every 60 days.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("read_later_app_menu_bookmarks_iph_trigger",
                                  Comparator(EQUAL, 0), 60, 60);
    config->used = EventConfig("read_later_bookmark_folder_opened",
                               Comparator(EQUAL, 0), 60, 60);
    config->event_configs.insert(
        EventConfig("read_later_article_saved",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 60, 60));
    return config;
  }

  if (kIPHReadLaterContextMenuFeature.name == feature->name) {
    // A config that allows the reading list label on the context menu to show
    // when the context menu "copy" option is clicked.
    // This will only occur once every 60 days.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("read_later_context_menu_tapped_iph_trigger",
                                  Comparator(EQUAL, 0), 60, 60);
    config->used = EventConfig("read_later_context_menu_tapped",
                               Comparator(EQUAL, 0), 60, 60);
    return config;
  }

  if (kIPHRequestDesktopSiteAppMenuFeature.name == feature->name) {
    // A config that allows the RDS site-level setting user education prompt to
    // be shown:
    // * If the user has used the RDS (tab-level) setting on the app menu at
    // least once.
    // * If the prompt has never been shown before.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("app_menu_desktop_site_for_tab_clicked",
                               Comparator(GREATER_THAN_OR_EQUAL, 1), 180, 180);
    config->trigger = EventConfig("request_desktop_site_app_menu_iph_trigger",
                                  Comparator(EQUAL, 0), 180, 180);
    return config;
  }

  if (kIPHRequestDesktopSiteDefaultOnFeature.name == feature->name) {
    // A config that allows the RDS default-on message to be shown:
    // * If the message has never been shown before.
    // * If the user has never accepted the message.
    // * If the user has never explicitly dismissed the message.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("desktop_site_settings_page_opened",
                               Comparator(ANY, 0), 360, 360);
    config->trigger = EventConfig("request_desktop_site_default_on_iph_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->event_configs.insert(
        EventConfig("desktop_site_default_on_primary_action",
                    Comparator(EQUAL, 0), 360, 360));
    config->event_configs.insert(EventConfig("desktop_site_default_on_gesture",
                                             Comparator(EQUAL, 0), 360, 360));
    return config;
  }

  if (kIPHRequestDesktopSiteOptInFeature.name == feature->name) {
    // A config that allows the RDS opt-in message to be shown:
    // * If the message has never been shown before.
    // * If the user has never accepted the message.
    // * If the user has never explicitly dismissed the message.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig("desktop_site_settings_page_opened",
                               Comparator(ANY, 0), 360, 360);
    config->trigger = EventConfig("request_desktop_site_opt_in_iph_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->event_configs.insert(EventConfig(
        "desktop_site_opt_in_primary_action", Comparator(EQUAL, 0), 360, 360));
    config->event_configs.insert(EventConfig("desktop_site_opt_in_gesture",
                                             Comparator(EQUAL, 0), 360, 360));
    return config;
  }

  if (kIPHRequestDesktopSiteExceptionsGenericFeature.name == feature->name) {
    // A config that allows the RDS site-level setting IPH to be shown to
    // tablet users. This will be triggered a maximum of 2 times (once per
    // 2 weeks), and if the user has not used the app menu to create a desktop
    // site exception in a span of a year.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 2);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->used = EventConfig("app_menu_desktop_site_exception_added",
                               Comparator(EQUAL, 0), 360, 360);
    config->trigger =
        EventConfig("request_desktop_site_exceptions_generic_iph_trigger",
                    Comparator(LESS_THAN, 2), 720, 720);
    config->event_configs.insert(
        EventConfig("request_desktop_site_exceptions_generic_iph_trigger",
                    Comparator(EQUAL, 0), 14, 14));
    return config;
  }

  if (kIPHPageZoomFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("page_zoom_iph_trigger", Comparator(EQUAL, 0), 1440, 1440);
    config->used =
        EventConfig("page_zoom_opened", Comparator(EQUAL, 0), 1440, 1440);
    return config;
  }

  if (kIPHRestoreTabsOnFREFeature.name == feature->name) {
    // A config that allows the restore tabs on FRE promo to be shown:
    // * If the user has gone through the FRE workflow.
    // * If the promo has never been accepted.
    // * Once per week if continually dismissed for a max of 2 weeks.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(LESS_THAN_OR_EQUAL, 14);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("restore_tabs_promo_trigger", Comparator(EQUAL, 0), 7, 7);
    config->used =
        EventConfig("restore_tabs_promo_used", Comparator(EQUAL, 0), 14, 14);
    config->event_configs.insert(EventConfig(
        "restore_tabs_on_first_run_show_promo", Comparator(EQUAL, 1), 14, 14));
    return config;
  }

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)

  if (kIPHAutofillExternalAccountProfileSuggestionFeature.name ==
      feature->name) {
    // Externally created account profile suggestion IPH is shown:
    // * once for an installation, 10-year window is used as the maximum
    // * if there was no address keyboard accessory IPH in the last 2 weeks
    // * if such a suggestion was not already accepted
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("autofill_external_account_profile_suggestion_iph_trigger",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->used =
        EventConfig("autofill_external_account_profile_suggestion_accepted",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);

#if BUILDFLAG(IS_ANDROID)
    config->event_configs.insert(
        EventConfig("keyboard_accessory_address_filling_iph_trigger",
                    Comparator(EQUAL, 0), 14, k10YearsInDays));
#endif  // BUILDFLAG(IS_ANDROID)

    return config;
  }

  if (kIPHAutofillVirtualCardSuggestionFeature.name == feature->name) {
    // A config that allows the virtual card credit card suggestion IPH to be
    // shown when:
    // * it has been shown less than three times in last 90 days;
    // * the virtual card suggestion has been selected less than twice in last
    // 90 days.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("autofill_virtual_card_iph_trigger",
                                  Comparator(LESS_THAN, 3), 90, 360);
    config->used = EventConfig("autofill_virtual_card_suggestion_accepted",
                               Comparator(LESS_THAN, 2), 90, 360);

#if BUILDFLAG(IS_ANDROID)
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    std::vector<std::string> affected_features;
    affected_features.push_back("IPH_KeyboardAccessoryBarSwiping");
    session_rate_impact.affected_features = affected_features;
    config->session_rate_impact = session_rate_impact;
#endif  // BUILDFLAG(IS_ANDROID)

    return config;
  }

  if (kIPHAutofillVirtualCardCVCSuggestionFeature.name == feature->name) {
    // A config that allows the virtual card CVC suggestion IPH to be
    // shown when:
    // * it has been shown less than three times in last 90 days;
    // * the virtual card CVC suggestion has been selected less than twice in
    // last 90 days.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    config->trigger = EventConfig("autofill_virtual_card_cvc_iph_trigger",
                                  Comparator(LESS_THAN, 3), 90, 360);
    config->used = EventConfig("autofill_virtual_card_cvc_suggestion_accepted",
                               Comparator(LESS_THAN, 2), 90, 360);
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    std::vector<std::string> affected_features;
    affected_features.push_back("IPH_AutofillVirtualCardSuggestion");

#if BUILDFLAG(IS_ANDROID)
    affected_features.push_back("IPH_KeyboardAccessoryBarSwiping");
#endif  // BUILDFLAG(IS_ANDROID)

    session_rate_impact.affected_features = affected_features;
    config->session_rate_impact = session_rate_impact;
    return config;
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_IOS)
  if (kIPHDefaultSiteViewFeature.name == feature->name) {
    // A config that shows an IPH on the overflow menu button advertising the
    // Default Page Mode feature when the user has requested the Desktop version
    // of a website 3 times in 60 days. It will be shown every other year unless
    // the user interacted with the setting in the past 2 years.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used =
        EventConfig("default_site_view_used", Comparator(EQUAL, 0), 720, 720);
    config->trigger =
        EventConfig("default_site_view_shown", Comparator(EQUAL, 0), 720, 720);
    config->event_configs.insert(
        EventConfig("desktop_version_requested",
                    Comparator(GREATER_THAN_OR_EQUAL, 3), 60, 60));
    return config;
  }

  if (kIPHWhatsNewFeature.name == feature->name) {
    // A config that allows a user education bubble to be shown for the bottom
    // toolbar. After the promo manager dismisses What's New promo, the user
    // education bubble will be shown once. This can only occur once every a
    // month.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("whats_new_trigger", Comparator(LESS_THAN, 1), 30, 360);
    config->used =
        EventConfig("whats_new_used", Comparator(LESS_THAN, 1), 30, 360);
    return config;
  }

  if (kIPHPriceNotificationsWhileBrowsingFeature.name == feature->name) {
    // A config that allows a user education bubble to be shown for the bottom
    // toolbar. The IPH will be displayed when the user is on a page with a
    // trackable product once per session for up to three sessions or until the
    // user has clicked on the Price Tracking entry point. There will be a
    // window of one week between impressions.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger = EventConfig("price_notifications_trigger",
                                  Comparator(LESS_THAN, 3), 730, 730);
    config->used =
        EventConfig("price_notifications_used", Comparator(EQUAL, 0), 730, 730);
    config->event_configs.insert(EventConfig("price_notifications_trigger",
                                             Comparator(EQUAL, 0), 7, 730));
    return config;
  }

  if (kIPHiOSDefaultBrowserBadgeEligibilityFeature.name == feature->name) {
    // A config for a shadow feature that is used to activate two other features
    // (kIPHiOSDefaultBrowserOverflowMenuBadgeFeature and
    // kIPHiOSDefaultBrowserSettingsBadgeFeature) which will enable a blue
    // notification badge to be shown to users at two different locations to
    // help bring their attention to the default browser settings page. This FET
    // feature is non-blocking because it is a passive promo that appears
    // alongside the rest of the UI, and does not interrupt the user's flow.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig("blue_dot_promo_eligibility_met",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("blue_dot_promo_criterion_met",
                               Comparator(GREATER_THAN_OR_EQUAL, 1), 30, 360);
    config->event_configs.insert(EventConfig("default_browser_promo_shown",
                                             Comparator(EQUAL, 0), 14, 360));
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSDefaultBrowserOverflowMenuBadgeFeature.name == feature->name) {
    // A config to allow a user to be shown the blue dot promo on the carousel.
    // It depends on kIPHiOSDefaultBrowserBadgeEligibilityFeature to have deemed
    // users eligible, and adds more constraints to decide when to stop showing
    // the promo to the user. This FET feature is non-blocking because it is a
    // passive promo that appears alongside the rest of the UI, and does not
    // interrupt the user's flow.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->used = EventConfig("blue_dot_promo_overflow_menu_dismissed",
                               Comparator(EQUAL, 0), 30, 360);
    config->trigger = EventConfig("blue_dot_promo_overflow_menu_shown",
                                  Comparator(ANY, 0), 360, 360);
    config->event_configs.insert(
        EventConfig("blue_dot_promo_overflow_menu_shown_new_session",
                    Comparator(LESS_THAN_OR_EQUAL, 2), 360, 360));
    config->event_configs.insert(
        EventConfig("blue_dot_promo_eligibility_met",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 30, 360));
    config->event_configs.insert(EventConfig("default_browser_promo_shown",
                                             Comparator(EQUAL, 0), 14, 360));
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSDefaultBrowserSettingsBadgeFeature.name == feature->name) {
    // A config to allow a user to be shown the blue dot promo in the default
    // browser settings row item. It depends on
    // kIPHiOSDefaultBrowserBadgeEligibilityFeature to have deemed users
    // eligible, and adds more constraints to decide when to stop showing the
    // promo. This FET feature is non-blocking because it is a passive promo
    // that appears alongside the rest of the UI, and does not interrupt the
    // user's flow.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->used = EventConfig("blue_dot_promo_settings_dismissed",
                               Comparator(EQUAL, 0), 30, 360);
    config->trigger = EventConfig("blue_dot_promo_settings_shown",
                                  Comparator(ANY, 0), 360, 360);
    config->event_configs.insert(
        EventConfig("blue_dot_promo_settings_shown_new_session",
                    Comparator(LESS_THAN_OR_EQUAL, 2), 360, 360));
    config->event_configs.insert(
        EventConfig("blue_dot_promo_eligibility_met",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 30, 360));
    config->event_configs.insert(EventConfig("default_browser_promo_shown",
                                             Comparator(EQUAL, 0), 14, 360));
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSNewTabToolbarItemFeature.name == feature->name) {
    // The IPH of the new tab button on the tool bar (at bottom on iPhone or on
    // top on iPad).
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // The user has opened the url from omnibox for >= 2 times in the past week.
    config->used = EventConfig(feature_engagement::events::kOpenUrlFromOmnibox,
                               Comparator(GREATER_THAN_OR_EQUAL, 2), 7, 7);
    // The IPH is shown at most 1 time a week.
    config->trigger = EventConfig("iph_new_tab_toolbar_item_trigger",
                                  Comparator(EQUAL, 0), 7, 7);
    // The user hasn't used the new tab toolbar item.
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kNewTabToolbarItemUsed,
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays));
    // The IPH is shown at most 2 times a year.
    config->event_configs.insert(EventConfig("iph_new_tab_toolbar_item_trigger",
                                             Comparator(LESS_THAN, 2), 365,
                                             365));
    return config;
  }

  if (kIPHiOSTabGridToolbarItemFeature.name == feature->name) {
    // The IPH of the tab grid button on the tool bar (at bottom on iPhone or on
    // top on iPad).
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // The user hasn't used the tab grid toolbar item.
    config->used =
        EventConfig(feature_engagement::events::kTabGridToolbarItemUsed,
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    // The IPH is shown at most 1 time a week.
    config->trigger = EventConfig("iph_tab_grid_toolbar_item_trigger",
                                  Comparator(EQUAL, 0), 7, 7);
    // The IPH is shown at most 2 times a year.
    config->event_configs.insert(
        EventConfig("iph_tab_grid_toolbar_item_trigger",
                    Comparator(LESS_THAN, 2), 365, 365));
    return config;
  }

  if (kIPHiOSHistoryOnOverflowMenuFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // The user hasn't tapped the history on the overflow menu.
    config->used =
        EventConfig(feature_engagement::events::kHistoryOnOverflowMenuUsed,
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    // The IPH is shown at most 1 time a week.
    config->trigger = EventConfig("history_on_overflow_menu_trigger",
                                  Comparator(EQUAL, 0), 7, 7);
    // The IPH is shown at most 2 times a year.
    config->event_configs.insert(EventConfig("history_on_overflow_menu_trigger",
                                             Comparator(LESS_THAN, 2), 365,
                                             365));
    // The user has opened URL from omnibox > 2 times in the past.
    config->event_configs.insert(EventConfig(
        feature_engagement::events::kOpenUrlFromOmnibox,
        Comparator(GREATER_THAN, 2), k10YearsInDays, k10YearsInDays));
    return config;
  }

  if (kIPHiOSShareToolbarItemFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // The user hasn't tapped the share on the toolbar.
    config->used =
        EventConfig(feature_engagement::events::kShareToolbarItemUsed,
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    // The IPH is shown at most 1 time a week.
    config->trigger =
        EventConfig("share_toolbar_item_trigger", Comparator(EQUAL, 0), 7, 7);
    // The IPH is shown 2 times a year.
    config->event_configs.insert(EventConfig(
        "share_toolbar_item_trigger", Comparator(LESS_THAN, 2), 365, 365));
    return config;
  }

  if (kIPHiOSPromoPasswordManagerWidgetFeature.name == feature->name) {
    // A config to allow a user to be shown the Password Manager widget promo in
    // the Password Manager. The promo will be shown for a maximum of three
    // subsequent Password Manager visits to users who have not yet installed
    // and used the widget. This FET feature is non-blocking because it is a
    // passive promo that appears alongside the rest of the UI, and does not
    // interrupt the user's flow.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig(
        feature_engagement::events::kPasswordManagerWidgetPromoTriggered,
        Comparator(LESS_THAN, 3), 360, 360);
    config->used =
        EventConfig(feature_engagement::events::kPasswordManagerWidgetPromoUsed,
                    Comparator(EQUAL, 0), 360, 360);
    config->event_configs.insert(EventConfig(
        feature_engagement::events::kPasswordManagerWidgetPromoClosed,
        Comparator(EQUAL, 0), 360, 360));
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  // iOS Promo Configs are split out into a separate file, so check that too.
  if (absl::optional<FeatureConfig> ios_promo_feature_config =
          GetClientSideiOSPromoFeatureConfig(feature)) {
    return ios_promo_feature_config;
  }
#endif  // BUILDFLAG(IS_IOS)

  if (kIPHDummyFeature.name == feature->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(LESS_THAN, 0);
    config->session_rate = Comparator(LESS_THAN, 0);
    config->trigger = EventConfig("dummy_feature_iph_trigger",
                                  Comparator(LESS_THAN, 0), 1, 1);
    config->used =
        EventConfig("dummy_feature_action", Comparator(LESS_THAN, 0), 1, 1);
    return config;
  }

  return absl::nullopt;
}

}  // namespace feature_engagement
