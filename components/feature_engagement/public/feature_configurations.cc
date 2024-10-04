// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_configurations.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/group_constants.h"

#if BUILDFLAG(IS_IOS)
#include "base/metrics/field_trial_params.h"
#include "components/feature_engagement/public/ios_promo_feature_configuration.h"
#endif  // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/feature_engagement/public/scalable_iph_feature_configurations.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
                       base::CompareCase::SENSITIVE)) {
    stripped_feature_name = stripped_feature_name.substr(strlen(prefix));
  }

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

#if BUILDFLAG(IS_IOS)
std::optional<FeatureConfig> CreateNewUserGestureInProductHelpConfig(
    const base::Feature& feature,
    const char* action_event,
    const char* trigger_event,
    const char* used_event,
    const char* dismiss_button_tap_event) {
  // Maximum storage days for iOS gesture IPHs in days. Note that they only
  // triggered for users who installed Chrome on iOS in the last specific number
  // of days, so this could be used as the maximum storage period of respective
  // events.
  const int kTotalMaxOccurrences = 2;
  const uint32_t kMaxStorageDays = 61;
  // The IPH only shows once a week, and honors `kTotalMaxOccurrences`.
  const int kDaysBetweenOccurrences = 7;

  std::optional<FeatureConfig> config = FeatureConfig();
  config->valid = true;
  config->availability = Comparator(ANY, 0);
  config->session_rate = Comparator(EQUAL, 0);
  // The user hasn't done the action suggested by the IPH.
  config->used = EventConfig(used_event, Comparator(EQUAL, 0), kMaxStorageDays,
                             kMaxStorageDays);
  // The IPH shows at most once per `kDaysBetweenOccurrences`.
  config->trigger =
      EventConfig(trigger_event, Comparator(EQUAL, 0), kDaysBetweenOccurrences,
                  kDaysBetweenOccurrences);
  config->event_configs.insert(
      EventConfig(trigger_event, Comparator(LESS_THAN, kTotalMaxOccurrences),
                  kMaxStorageDays, kMaxStorageDays));
  // The IPH only shows when user performs the action that should trigger the
  // IPH at least twice since the last time the IPH shows, or since installation
  // if it hasn't.
  config->event_configs.insert(
      EventConfig(action_event, Comparator(GREATER_THAN_OR_EQUAL, 2),
                  kDaysBetweenOccurrences, kDaysBetweenOccurrences));
  // The user hasn't explicitly dismissed the same IPH before.
  config->event_configs.insert(EventConfig(dismiss_button_tap_event,
                                           Comparator(EQUAL, 0),
                                           kMaxStorageDays, kMaxStorageDays));
  return config;
}
#endif

std::optional<FeatureConfig> GetClientSideFeatureConfig(
    const base::Feature* feature) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

  // The IPH bubble for link capturing has a trigger set to ANY so that it
  // always shows up. The per app specific guardrails are independently stored
  // under the web_app_prefs.
  if (kIPHDesktopPWAsLinkCapturingLaunch.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("desktop_pwa_launch_link_capturing",
                                  Comparator(ANY, 0), 0, 0);
    config->used = EventConfig("desktop_pwa_launch_link_capturing_used",
                               Comparator(ANY, 0), 0, 0);
    return config;
  }

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (kIPHPasswordsManagementBubbleAfterSaveFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->trigger =
        EventConfig("password_saved", Comparator(LESS_THAN, 1), 360, 360);
    config->session_rate = Comparator(ANY, 0);
    config->availability = Comparator(ANY, 0);
    return config;
  }

  if (kIPHPasswordsManagementBubbleDuringSigninFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->trigger =
        EventConfig("signin_flow_detected", Comparator(LESS_THAN, 1), 360, 360);
    config->session_rate = Comparator(ANY, 0);
    config->availability = Comparator(ANY, 0);
    return config;
  }

  if (kIPHProfileSwitchFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHSidePanelGenericPinnableFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the promo once a year if the side panel was not opened.
    config->trigger = EventConfig("side_panel_pinnable_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig(feature_engagement::events::kSidePanelPinned,
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHSignoutWebInterceptFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig("iph_signout_web_intercept_triggered",
                                  Comparator(ANY, 0), 0, 0);
    config->used =
        EventConfig("iph_signout_web_intercept_used", Comparator(ANY, 0), 0, 0);
    return config;
  }

  if (kIPHGMCCastStartStopFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("gmc_start_stop_iph_trigger",
                                  Comparator(EQUAL, 0), 180, 180);
    config->used = EventConfig("media_route_stopped_from_gmc",
                               Comparator(EQUAL, 0), 180, 180);
    return config;
  }

  if (kIPHGMCLocalMediaCastingFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig("gmc_local_media_cast_iph_trigger",
                                  Comparator(EQUAL, 0), 180, 180);
    config->used = EventConfig("media_route_started_from_gmc",
                               Comparator(EQUAL, 0), 180, 180);

    return config;
  }

  if (kIPHDesktopSharedHighlightingFeature.name == feature->name) {
    // A config that allows the shared highlighting desktop IPH to be shown
    // when a user receives a highlight:
    // * Once per 7 days
    // * Up to 5 times but only if unused in the last 7 days.
    // * Used fewer than 2 times

    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHExperimentalAIPromoFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // Show the IPH once per year.
    config->trigger = EventConfig("iph_experimental_ai_promo_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("iph_experimental_ai_promo_shown",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHBatterySaverModeFeature.name == feature->name) {
    // Show promo once a year when the battery saver toolbar icon is visible.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("battery_saver_info_triggered",
                                  Comparator(LESS_THAN, 1), 360, 360);
    config->used =
        EventConfig("battery_saver_info_shown", Comparator(EQUAL, 0), 7, 360);
    return config;
  }

  if (kIPHMemorySaverModeFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
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

  if (kIPHPerformanceInterventionDialogFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show intervention dialog at most once per day and no more than 5 times
    // per week.
    config->trigger = EventConfig("performance_intervention_dialog_trigger",
                                  Comparator(EQUAL, 0), 1, 360);
    config->event_configs.insert(
        EventConfig("performance_intervention_dialog_trigger",
                    Comparator(LESS_THAN, 5), 7, 360));
    return config;
  }

  if (kIPHPowerBookmarksSidePanelFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the label once per day, 3 times max in 28 days.
    config->trigger =
        EventConfig("price_insights_page_action_icon_label_in_trigger",
                    Comparator(ANY, 0), 0, 360);
    config->used = EventConfig("price_insights_page_action_icon_label_used",
                               Comparator(ANY, 0), 0, 360);
    config->event_configs.insert(
        EventConfig("price_insights_page_action_icon_label_in_trigger",
                    Comparator(ANY, 0), 0, 360));
    return config;
  }

  if (kIPHPriceTrackingEmailConsentFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the IPH 3 times per year.
    config->trigger = EventConfig("shopping_collection_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    return config;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (kIPHExtensionsMenuFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
#endif

  if (kIPHCompanionSidePanelFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;

    // Show the promo up to 3 times a year.
    config->trigger = EventConfig("iph_companion_side_panel_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used =
        EventConfig("companion_side_panel_accessed_via_toolbar_button",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHCompanionSidePanelRegionSearchFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHPasswordsWebAppProfileSwitchFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("iph_password_manager_shortcut_triggered",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("password_manager_shortcut_created",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHPasswordSharingFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig("password_sharing_iph_triggered",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("password_share_button_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHDiscardRingFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("discard_ring_trigger", Comparator(EQUAL, 0), 360, 360);
    // This event is never logged but is included for consistency.
    config->used =
        EventConfig("discard_ring_used", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHDownloadEsbPromoFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Don't show if user has already seen an IPH this session.
    // Show the promo max once a year if the user hasn't interacted with
    // a dangerous download within the last 21 days.
    config->trigger = EventConfig("download_bubble_esb_iph_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("enable_enhanced_protection",
                               Comparator(EQUAL, 0), 21, 360);
    config->event_configs.insert(
        EventConfig("download_bubble_dangerous_download_detected",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 21, 360));
    return config;
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (kEsbDownloadRowPromoFeature.name == feature->name) {
    // A config that allows a promotion row referring users to enable Enhanced
    // Safe Browsing (ESB), to be shown on the Downloads manager page. It
    // can be viewed at most 7 times without interaction across a 90 day period.
    // If the user clicks, then we aritificially increment the viewed event by 4
    // so that the row can be seen at most 2 more times.
    //
    // The trigger management can be found in
    // c/b/ui/webui/downloads/downloads_dom_handler.cc
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);

    // This isn't an IPH so we don't suppress other engagement features.
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->session_rate_impact = session_rate_impact;

    // This isn't an IPH so we don't want to block or be blocked by any other
    // engagement features.
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;

    config->trigger = EventConfig("dangerous_download_esb_promo_row_trigger",
                                  Comparator(ANY, 0), 360, 360);
    config->used =
        EventConfig("enable_enhanced_protection", Comparator(EQUAL, 0), 21, 90);
    config->event_configs.insert(EventConfig("esb_download_promo_row_viewed",
                                             Comparator(LESS_THAN, 7), 90, 90));
    config->event_configs.insert(EventConfig("esb_download_promo_row_clicked",
                                             Comparator(LESS_THAN, 3), 90, 90));

    return config;
  }
#endif

  if (kIPHBackNavigationMenuFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHComposeNewBadgeFeature.name == feature->name) {
    // A config that allows the new badge displayed in the Compose feature nudge
    // to be shown at most 4 times in a 10-day window and only while the user
    // has opened the Compose feature less than 3 times.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("compose_new_badge_triggered",
                                  Comparator(LESS_THAN, 4), 10, 360);
    config->used =
        EventConfig("compose_activated", Comparator(LESS_THAN, 3), 360, 360);
    return config;
  }

  if (kIPHComposeMSBBSettingsFeature.name == feature->name) {
    // A config that allows a toast to be displayed in the Settings page when
    // opened via the Compose MSBB feature
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig("compose_msbb_settings_feature_trigger",
                                  Comparator(ANY, 0), 90, 90);
    config->used = EventConfig("compose_msbb_settings_feature_used",
                               Comparator(ANY, 0), 90, 90);
    return config;
  }

  if (kIPHTabOrganizationSuccessFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the IPH once per year.
    config->trigger = EventConfig("iph_tab_organization_success_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used =
        EventConfig("tab_group_editor_shown", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (kIPHiOSPasswordPromoDesktopFeature.name == feature->name) {
    // A config for allowing other IPH's to explicitly block the iOS password
    // promo bubble on desktop if needed. Blocked and blocking by default, so
    // won't appear at the same time as other IPH, but without any session rate
    // impact.

    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHiOSAddressPromoDesktopFeature.name == feature->name) {
    // A config for allowing other IPH's to explicitly block the iOS address
    // promo bubble on desktop if needed. Blocked and blocking by default, so
    // won't appear at the same time as other IPH, but without any session rate
    // impact.

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::ALL;
    config->blocking.type = Blocking::Type::ALL;
    config->used =
        EventConfig("ios_address_promo_bubble_on_desktop_interacted_with",
                    Comparator(ANY, 0), 0, 0);
    config->trigger = EventConfig("ios_address_promo_bubble_on_desktop_shown",
                                  Comparator(ANY, 0), 0, 0);
    return config;
  }

  if (kIPHiOSPaymentPromoDesktopFeature.name == feature->name) {
    // A config for allowing other IPH's to explicitly block the iOS payment
    // promo bubble on desktop if needed. Blocked and blocking by default, so
    // won't appear at the same time as other IPH, but without any session rate
    // impact.

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::ALL;
    config->blocking.type = Blocking::Type::ALL;
    config->used =
        EventConfig("ios_payment_promo_bubble_on_desktop_interacted_with",
                    Comparator(ANY, 0), 0, 0);
    config->trigger = EventConfig("ios_payment_promo_bubble_on_desktop_shown",
                                  Comparator(ANY, 0), 0, 0);
    return config;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_ANDROID)

  if (kIPHAndroidTabDeclutter.name == feature->name) {
    // Allows an IPH for tab declutter for the tab switcher button:
    // * Only once per week.
    // * Up to 3 times per year.
    // * And only as long as the user has never manually accessed their
    //   archived tabs.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("android_tab_declutter_iph_triggered",
                                  Comparator(EQUAL, 0), 7, 7);
    config->event_configs.insert(
        EventConfig("android_tab_declutter_iph_triggered",
                    Comparator(LESS_THAN, 3), 360, 360));
    config->used = EventConfig("android_tab_declutter_button_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHTabGroupSyncOnStripFeature.name == feature->name) {
    // A config that allows the TabGroupSync IPH to be shown up to 3 times per
    // year.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger = EventConfig("tab_group_sync_on_strip_iph_triggered",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used = EventConfig("tab_groups_surface_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHTabGroupCreationDialogSyncTextFeature.name == feature->name) {
    // A config that allows the sync text IPH on the TabGroupCreationDialog to
    // be shown up to 3 times total (10 year max in place of unlimited window).
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("tab_group_creation_dialog_sync_text_iph_triggered",
                    Comparator(LESS_THAN, 3), 3600, 3600);
    config->used = EventConfig("tab_group_creation_dialog_shown",
                               Comparator(LESS_THAN, 3), 3600, 3600);
    return config;
  }

  if (kIPHAppSpecificHistory.name == feature->name) {
    // A config that allows the AppSpecificHistory IPH to be shown once
    // a week, up to 3 times, unless the button is clicked at least once.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("app_specific_history_iph_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->event_configs.insert(EventConfig("app_specific_history_iph_trigger",
                                             Comparator(LESS_THAN, 1), 7, 360));
    config->used = EventConfig("history_toolbar_search_menu_item_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHCCTHistory.name == feature->name) {
    // A config that allows the CCTHistory IPH to be shown once
    // a week, up to 3 times, unless the button is clicked at least once.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("cct_history_iph_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->event_configs.insert(EventConfig("cct_history_iph_trigger",
                                             Comparator(LESS_THAN, 1), 7, 360));
    config->used = EventConfig("cct_history_menu_item_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHCCTMinimized.name == feature->name) {
    // A config that allows the Custom Tab minimize button IPH to be shown once
    // a day, up to 3 times, unless the button is clicked at least once.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("cct_minimized_iph_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->event_configs.insert(EventConfig("cct_minimized_iph_trigger",
                                             Comparator(LESS_THAN, 1), 1, 360));
    config->used = EventConfig("cct_minimize_button_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHDataSaverDetailFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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

  // A generic feature that always returns true.
  if (kIPHGenericAlwaysTriggerHelpUiFeature.name == feature->name) {
    return CreateAlwaysTriggerConfig(feature);
  }

  if (kIPHLowUserEngagementDetectorFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
  if (kIPHTabGroupsDragAndDropFeature.name == feature->name) {
    // IPH for drag & drop promo in the Tab Switcher.
    // * Drag & drop has not been used previously in the last 360 days.
    // * Iff IPH has been shown 0 times in 30 days AND less than 2 times in 90
    // days window.
    // * Once per session.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger = EventConfig("iph_tabgroups_drag_and_drop",
                                  Comparator(EQUAL, 0), 30, 360);
    config->event_configs.insert(EventConfig(
        "iph_tabgroups_drag_and_drop", Comparator(LESS_THAN, 2), 90, 360));
    config->used = EventConfig("tab_drag_and_drop_to_group",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHTabGroupsRemoteGroupFeature.name == feature->name) {
    // Allows an IPH for highlighting a remote tab group when:
    // * Only once per day.
    // * Up to 3 times per year.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger = EventConfig("tab_groups_remote_group_triggered",
                                  Comparator(EQUAL, 0), 1, 1);
    config->event_configs.insert(
        EventConfig("tab_groups_remote_group_triggered",
                    Comparator(LESS_THAN, 3), 360, 360));
    return config;
  }
  if (kIPHTabGroupsSurfaceFeature.name == feature->name) {
    // Allows an IPH for the tab groups surface through hub toolbar when:
    // * Only once per week.
    // * Up to 3 times per year.
    // * And only as long as the user has never manually opened the tab groups
    // surface from the hub toolbar.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger =
        EventConfig("tab_groups_surface_triggered", Comparator(EQUAL, 0), 7, 7);
    config->event_configs.insert(EventConfig(
        "tab_groups_surface_triggered", Comparator(LESS_THAN, 3), 360, 360));
    config->used = EventConfig("tab_groups_surface_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHTabGroupsSurfaceOnHideFeature.name == feature->name) {
    // Allows an IPH for the tab groups surface when hiding a tab group when:
    // * Only once per year.
    // * And only as long as the user has never manually opened the tab groups
    // surface from the hub toolbar.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger = EventConfig("tab_groups_surface_on_hide_triggered",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used = EventConfig("tab_groups_surface_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHTabSwitcherButtonFeature.name == feature->name) {
    // Adjusted to be less spammy for users that may know what the tab switcher
    // is. Show after 14 days of Chrome being installed, once every 90 days,
    // unless the user has used the tab switcher button in the last year.
    // Hopefully a year will be long enough that infrequent users of a given
    // Chrome channel should almost never see it.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger =
        EventConfig("tab_switcher_iph_triggered", Comparator(EQUAL, 0), 90, 90);
    config->used = EventConfig("tab_switcher_button_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    config->snooze_params.snooze_interval = 7;
    config->snooze_params.max_limit = 3;
    return config;
  }
  if (kIPHTabSwitcherButtonSwitchIncognitoFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger = EventConfig("tab_switcher_switch_incognito_iph_triggered",
                                  Comparator(EQUAL, 0), 90, 90);
    config->used = EventConfig("tab_switcher_button_long_clicked",
                               Comparator(EQUAL, 0), 14, 90);
    config->snooze_params.snooze_interval = 7;
    config->snooze_params.max_limit = 3;
    return config;
  }
  if (kIPHTabSwitcherFloatingActionButtonFeature.name == feature->name) {
    // Allows an IPH for the tab groups surface through hub toolbar when:
    // * Only once per week.
    // * Up to 3 times per year.
    // * And only as long as the user has never manually pressed the
    // floating new tab button.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger =
        EventConfig("tab_switcher_floating_action_button_iph_triggered",
                    Comparator(EQUAL, 0), 7, 7);
    config->event_configs.insert(
        EventConfig("tab_switcher_floating_action_button_iph_triggered",
                    Comparator(LESS_THAN, 3), 360, 360));
    config->used = EventConfig("tab_switcher_floating_action_button_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHWebFeedFollowFeature.name == feature->name) {
    // A config that allows the WebFeed follow intro to be shown up to 5x per
    // week.
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("web_feed_post_follow_dialog_trigger",
                                  Comparator(LESS_THAN, 3), 360, 360);
    config->used = EventConfig("web_feed_post_follow_dialog_shown",
                               Comparator(ANY, 0), 360, 360);
    return config;
  }

  if (kIPHWebFeedPostFollowDialogFeatureWithUIUpdate.name == feature->name) {
    // A config that allows one of the WebFeed post follow dialogs to be
    // presented 3 times after the UI update.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("web_feed_post_follow_dialog_trigger_with_ui_update",
                    Comparator(LESS_THAN, 3), 360, 360);
    config->used =
        EventConfig("web_feed_post_follow_dialog_shown_with_ui_update",
                    Comparator(ANY, 0), 360, 360);
    return config;
  }

  if (kIPHVideoTutorialNTPChromeIntroFeature.name == feature->name) {
    // A config that allows the chrome intro video tutorial card to show up
    // until explicitly interacted upon.
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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

    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    // TODO(crbug.com/40198496): Update this config from test values; Will
    // likely depend on giving feedback instead of opening settings, since the
    // primary purpose  of the dialog has changed.
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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

    std::optional<FeatureConfig> config = FeatureConfig();
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

    std::optional<FeatureConfig> config = FeatureConfig();
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

    std::optional<FeatureConfig> config = FeatureConfig();
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

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("read_later_context_menu_tapped_iph_trigger",
                                  Comparator(EQUAL, 0), 60, 60);
    config->used = EventConfig("read_later_context_menu_tapped",
                               Comparator(EQUAL, 0), 60, 60);
    return config;
  }

  if (kIPHRequestDesktopSiteDefaultOnFeature.name == feature->name) {
    // A config that allows the RDS default-on message to be shown:
    // * If the user has never accepted the message.
    // * The message can show twice, but only once in a week.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(GREATER_THAN, 1);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->used = EventConfig("desktop_site_settings_page_opened",
                               Comparator(ANY, 0), 360, 360);
    config->trigger = EventConfig("request_desktop_site_default_on_iph_trigger",
                                  Comparator(LESS_THAN_OR_EQUAL, 1), 360, 360);
    config->event_configs.insert(
        EventConfig("request_desktop_site_default_on_iph_trigger",
                    Comparator(EQUAL, 0), 7, 360));
    config->event_configs.insert(
        EventConfig("desktop_site_default_on_primary_action",
                    Comparator(EQUAL, 0), 360, 360));
    config->event_configs.insert(EventConfig("desktop_site_default_on_gesture",
                                             Comparator(ANY, 0), 360, 360));
    return config;
  }

  if (kIPHRequestDesktopSiteExceptionsGenericFeature.name == feature->name) {
    // A config that allows the RDS site-level setting IPH to be shown to
    // tablet users. This will be triggered a maximum of 2 times (once per
    // 2 weeks), and if the user has not used the app menu to create a desktop
    // site exception in a span of a year.
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
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
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(LESS_THAN_OR_EQUAL, 14);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("restore_tabs_promo_trigger", Comparator(EQUAL, 0), 7, 14);
    config->used =
        EventConfig("restore_tabs_promo_used", Comparator(EQUAL, 0), 14, 14);
    config->event_configs.insert(
        EventConfig("restore_tabs_on_first_run_show_promo",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 14, 14));
    return config;
  }

  if (kIPHRequestDesktopSiteWindowSettingFeature.name == feature->name) {
    // A config that allows the RDS window setting IPH to be shown at most once
    // in 3 years per device.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->used = EventConfig("request_desktop_site_window_setting_iph_shown",
                               Comparator(EQUAL, 0), 1080, 1080);
    config->trigger =
        EventConfig("request_desktop_site_window_setting_iph_trigger",
                    Comparator(EQUAL, 0), 1080, 1080);
    return config;
  }

  if (kIPHReadAloudExpandedPlayerFeature.name == feature->name) {
    // Show tooltip at most 3 times, once a day, but stop if user saw
    // expanded player.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->used = EventConfig("read_aloud_expanded_player_shown",
                               Comparator(EQUAL, 0), 360, 360);
    config->trigger =
        EventConfig("read_aloud_expanded_player_shown_iph_trigger",
                    Comparator(EQUAL, 0), 1, 1);
    config->event_configs.insert(
        EventConfig("read_aloud_expanded_player_shown_iph_trigger",
                    Comparator(LESS_THAN, 3), 360, 360));
    return config;
  }

  if (kIPHAutofillDisabledVirtualCardSuggestionFeature.name == feature->name) {
    // A config that allows the virtual card disabled suggestion IPH to be shown
    // when it has been shown less than three times in last 90 days.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->trigger = EventConfig("autofill_disabled_virtual_card_iph_trigger",
                                  Comparator(LESS_THAN, 3), 90, 360);

    // This promo blocks specific promos in the same session.
    config->session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    config->session_rate_impact.affected_features.emplace();
    config->session_rate_impact.affected_features->push_back(
        "IPH_AutofillVirtualCardSuggestion");
    config->session_rate_impact.affected_features->push_back(
        "IPH_KeyboardAccessoryBarSwiping");

    return config;
  }

  if (kIPHDefaultBrowserPromoMagicStackFeature.name == feature->name) {
    // A config that allows the default browser promo Magic Stack to be shown:
    // * Up to 3 times
    // * Neither Magic Stack, Messages, nor the settings card has been triggered
    // in the last 7 days.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->trigger =
        EventConfig("default_browser_promo_magic_stack_trigger",
                    Comparator(LESS_THAN, 3), k10YearsInDays, k10YearsInDays);
    config->groups.push_back(kClankDefaultBrowserPromosGroup.name);

    return config;
  }

  if (kIPHDefaultBrowserPromoMessagesFeature.name == feature->name) {
    // A config that allows the default browser promo messages to be shown:
    // * Up to 2 times
    // * Neither Magic Stack, Messages, nor the settings card has been triggered
    // in the last 7 days.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->trigger =
        EventConfig("default_browser_promo_messages_trigger",
                    Comparator(LESS_THAN, 2), k10YearsInDays, k10YearsInDays);
    config->used = EventConfig("default_browser_promo_messages_used",
                               Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("default_browser_promo_messages_dismissed",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays));
    config->groups.push_back(kClankDefaultBrowserPromosGroup.name);
    return config;
  }

  if (kIPHDefaultBrowserPromoSettingCardFeature.name == feature->name) {
    // A config that allows the default browser promo setting card to be shown:
    // * Up to 4 times
    // * Neither Magic Stack, Messages, nor the settings card has been triggered
    // in the last 7 days.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->trigger =
        EventConfig("default_browser_promo_setting_card_trigger",
                    Comparator(LESS_THAN, 4), k10YearsInDays, k10YearsInDays);
    config->used = EventConfig("default_browser_promo_setting_card_used",
                               Comparator(ANY, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("default_browser_promo_setting_card_dismissed",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays));
    config->groups.push_back(kClankDefaultBrowserPromosGroup.name);

    return config;
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)

  if (kIPHAutofillCreditCardBenefitFeature.name == feature->name) {
    // Credit card benefit IPH is shown:
    // * once for an installation, 10-year window is used as the maximum
    // * when a credit card benefit is displayed for the first time
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger =
        EventConfig("autofill_credit_card_benefit_iph_trigger",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->used =
        EventConfig("autofill_credit_card_benefit_iph_accepted",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHAutofillExternalAccountProfileSuggestionFeature.name ==
      feature->name) {
    // Externally created account profile suggestion IPH is shown:
    // * once for an installation, 10-year window is used as the maximum
    // * if there was no address keyboard accessory IPH in the last 2 weeks
    // * if such a suggestion was not already accepted
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHAutofillManualFallbackFeature.name == feature->name) {
    // Autofill Manual Fallback IPH is shown if all of the following are true:
    // * it has not been shown before in the last 90 days;
    // * the user has never used the autofill manual fallback.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig("autofill_manual_fallback_trigger",
                                  Comparator(LESS_THAN, 1), 90, 360);
    config->used =
        EventConfig("autofill_manual_fallback_accepted", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHAutofillVirtualCardSuggestionFeature.name == feature->name) {
    // A config that allows the virtual card credit card suggestion IPH to be
    // shown when:
    // * it has been shown less than three times in last 90 days;
    // * the virtual card suggestion has been selected less than twice in last
    // 90 days.

    std::optional<FeatureConfig> config = FeatureConfig();
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

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
#if BUILDFLAG(IS_ANDROID)
    config->session_rate = Comparator(EQUAL, 0);
#else
    // On desktop, toasts should always be available.
    config->availability = Comparator(ANY, 0);
#endif
    config->trigger = EventConfig("autofill_virtual_card_cvc_iph_trigger",
                                  Comparator(LESS_THAN, 3), 90, 360);
    config->used = EventConfig("autofill_virtual_card_cvc_suggestion_accepted",
                               Comparator(LESS_THAN, 2), 90, 360);

    // This promo blocks specific promos in the same session.
    config->session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    config->session_rate_impact.affected_features.emplace();
    config->session_rate_impact.affected_features->push_back(
        "IPH_AutofillVirtualCardSuggestion");
#if BUILDFLAG(IS_ANDROID)
    config->session_rate_impact.affected_features->push_back(
        "IPH_KeyboardAccessoryBarSwiping");
#endif  // BUILDFLAG(IS_ANDROID)

    return config;
  }

  if (kIPHCookieControlsFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    // Show promo up to 3 times per year and only if user hasn't interacted with
    // the cookie controls bubble in the last week.
    config->trigger = EventConfig("iph_cookie_controls_triggered",
                                  Comparator(LESS_THAN, 3), 360, 360);
#if !BUILDFLAG(IS_ANDROID)
    config->used =
        EventConfig(feature_engagement::events::kCookieControlsBubbleShown,
                    Comparator(EQUAL, 0), 7, 7);
#endif  // !BUILDFLAG(IS_ANDROID)
    return config;
  }

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_IOS)
  if (kIPHiOSLensOverlayEntrypointTipFeature.name == feature->name) {
    // A config that allows the Lens overlay IPH to be shown to users. This will
    // be triggered a maximum of 2 times (once per week), and if the user has
    // not used lens overlay.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);

    constexpr char kLensOverlayFeatureTriggerEvent[] =
        "lens_overlay_feature_trigger";

    config->trigger =
        EventConfig(kLensOverlayFeatureTriggerEvent, Comparator(LESS_THAN, 2),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);

    config->event_configs.emplace(kLensOverlayFeatureTriggerEvent,
                                  Comparator(EQUAL, 0), 7, 7);

    config->used =
        EventConfig(feature_engagement::events::kLensOverlayEntrypointUsed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);

    return config;
  }
  if (kIPHiOSContextualPanelPriceInsightsFeature.name == feature->name) {
    // The contextual panel's price insights entrypoint IPH config to control
    // the impressions of the IPH for this infoblock. Shows the IPH 3 times
    // every 6 months (max 1 per day), for a maximum of 6 times lifetime. Stops
    // showing the IPH if the entrypoint was used, or explicitly dismissed
    // twice.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->used = EventConfig(
        feature_engagement::events::
            kIOSContextualPanelPriceInsightsEntrypointUsed,
        Comparator(LESS_THAN, 1), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    config->used = EventConfig(
        feature_engagement::events::
            kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed,
        Comparator(LESS_THAN, 2), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    config->trigger = EventConfig(
        "ios_contextual_panel_price_insights_entrypoint_iph_trigger",
        Comparator(LESS_THAN, 3), 182, feature_engagement::kMaxStoragePeriod);
    config->event_configs.insert(EventConfig(
        "ios_contextual_panel_price_insights_entrypoint_iph_trigger",
        Comparator(LESS_THAN, 1), 1, feature_engagement::kMaxStoragePeriod));
    config->event_configs.insert(EventConfig(
        "ios_contextual_panel_price_insights_entrypoint_iph_trigger",
        Comparator(LESS_THAN, 6), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod));

    // This IPH is blocked by the overflow menu's price tracking IPH
    // (kIPHPriceNotificationsWhileBrowsingFeature) if shown in the same
    // session (approximated by checking for an event with a 1 day lookback
    // window). Done through an event config since session rate is ignored.
    config->event_configs.insert(EventConfig("price_notifications_trigger",
                                             Comparator(LESS_THAN, 1), 1, 1));

    // This IPH blocks the overflow menu's price tracking IPH
    // (kIPHPriceNotificationsWhileBrowsingFeature) if shown in the same
    // session.
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    std::vector<std::string> affected_features;
    affected_features.push_back("IPH_PriceNotificationsWhileBrowsing");
    session_rate_impact.affected_features = affected_features;
    config->session_rate_impact = session_rate_impact;

    return config;
  }

  if (kIPHiOSContextualPanelSampleModelFeature.name == feature->name) {
    // The contextual panel's sample model entrypoint IPH config to control the
    // impressions of the IPH for this infoblock. Shows the IPH up to 3 times
    // per day if the user doesn't interact with the entrypoint or explicitly
    // dismiss it, and is blocking/blocked to/by all other IPHs.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->used = EventConfig(feature_engagement::events::
                                   kIOSContextualPanelSampleModelEntrypointUsed,
                               Comparator(LESS_THAN, 1), 1, 1);
    config->trigger =
        EventConfig("ios_contextual_panel_sample_model_entrypoint_iph_trigger",
                    Comparator(LESS_THAN, 3), 1, 1);
    config->event_configs.insert(EventConfig(
        "ios_contextual_panel_sample_model_entrypoint_explicitly_dismissed",
        Comparator(LESS_THAN, 1), 1, 1));
    return config;
  }

  if (kIPHDefaultSiteViewFeature.name == feature->name) {
    // A config that shows an IPH on the overflow menu button advertising the
    // Default Page Mode feature when the user has requested the Desktop version
    // of a website 3 times in 60 days. It will be shown every other year unless
    // the user interacted with the setting in the past 2 years.
    std::optional<FeatureConfig> config = FeatureConfig();
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

    std::optional<FeatureConfig> config = FeatureConfig();
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

    // This IPH is blocked by the Contextual Panel's price insights entrypoint
    // IPH (kIPHiOSContextualPanelPriceInsightsFeature) via explicit session
    // rate blocking.
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHiOSDefaultBrowserOverflowMenuBadgeFeature.name == feature->name) {
    // A config to allow a user to be shown the blue dot promo on the Chrome
    // Settings icon in overflow menu and Default Browser row in Chrome
    // Settings.
    // This FET feature is non-blocking because it is a passive promo
    // that appears alongside the rest of the UI, and does not interrupt the
    // user's flow.

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig("blue_dot_promo_overflow_menu_shown",
                                  Comparator(ANY, 0), 360, 360);
    // Stop showing blue dot promo if overflow menu opened while blue dot was
    // showing at least 3 times.
    config->used = EventConfig("blue_dot_promo_overflow_menu_opened",
                               Comparator(LESS_THAN, 3), 360, 360);
    // Stop showing blue dot promo if default browser settings was opened at
    // least once.
    config->event_configs.insert(EventConfig(
        "blue_dot_promo_settings_dismissed", Comparator(EQUAL, 0), 360, 360));
    // Stop showing blue dot promo if overflow menu was customized while blue
    // dot was showing.
    config->event_configs.insert(EventConfig(
        "blue_dot_overflow_menu_customized", Comparator(EQUAL, 0), 360, 360));

    // Cooldowns from other default browser promos.
    config->event_configs.insert(EventConfig("default_browser_promo_shown",
                                             Comparator(EQUAL, 0), 14, 360));
    config->event_configs.insert(EventConfig("default_browser_fre_shown",
                                             Comparator(EQUAL, 0), 21, 360));
    config->event_configs.insert(EventConfig(
        "default_browser_promos_group_trigger", Comparator(EQUAL, 0), 14, 360));
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kChromeOpened,
                    Comparator(GREATER_THAN_OR_EQUAL, 7), 360, 360));

    // Continue checking deprecated settings badge conditions to not show blue
    // dot at all if user would not have qualified for settings badge.
    // TODO(crbug.com/362504599): Remove in July 2025.
    config->event_configs.insert(
        EventConfig("blue_dot_promo_settings_shown_new_session",
                    Comparator(LESS_THAN_OR_EQUAL, 2), 360, 360));
    // TODO(crbug.com/362504058): Remove in Sept 2025.
    config->event_configs.insert(
        EventConfig("blue_dot_promo_overflow_menu_dismissed",
                    Comparator(LESS_THAN, 3), 360, 360));

    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSHistoryOnOverflowMenuFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
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

  if (kIPHiOSPromoPasswordManagerWidgetFeature.name == feature->name) {
    // A config to allow a user to be shown the Password Manager widget promo in
    // the Password Manager. The promo will be shown for a maximum of three
    // subsequent Password Manager visits to users who have not yet installed
    // and used the widget. This FET feature is non-blocking because it is a
    // passive promo that appears alongside the rest of the UI, and does not
    // interrupt the user's flow.

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->trigger = EventConfig(
        feature_engagement::events::kPasswordManagerWidgetPromoTriggered,
        Comparator(LESS_THAN, 2), 360, 360);
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

  if (kIPHiOSLensKeyboardFeature.name == feature->name) {
    // A config that allows a user education bubble to be shown for the Lens
    // button in the omnibox keyboard. Will be shown up to 3 times, but
    // opening Lens from the keyboard will prevent the bubble from appearing
    // again.

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("lens_keyboard_feature_trigger", Comparator(LESS_THAN, 3),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->used = EventConfig("lens_keyboard_used", Comparator(EQUAL, 0),
                               feature_engagement::kMaxStoragePeriod,
                               feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSParcelTrackingFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // The IPH is shown at most once.
    config->trigger =
        EventConfig(feature_engagement::events::kParcelTrackingTriggered,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->used =
        EventConfig("parcel_tracking_feature_used", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // The user has tracked a parcel.
    config->event_configs.insert(
        EventConfig(feature_engagement::events::kParcelTracked,
                    Comparator(GREATER_THAN_OR_EQUAL, 1),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod));
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSPullToRefreshFeature.name == feature->name) {
    // The IPH of the pull-to-refresh feature for the current tab.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSMultiGestureRefreshUsed,
        /*trigger_event=*/"iph_pull_to_refresh_trigger", /*used_event=*/
        feature_engagement::events::kIOSPullToRefreshUsed,
        /*dismiss_button_tap_event=*/
        feature_engagement::events::kIOSPullToRefreshIPHDismissButtonTapped);
  }

  if (kIPHiOSReplaceSyncPromosWithSignInPromos.name == feature->name) {
    // A config to show a user education bubble from the account row in the
    // settings page. Will be shown only the first time user signs-in from
    // settings. Subsequent sign-ins will not trigger it.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("signin_from_settings_trigger", Comparator(LESS_THAN, 1),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->used =
        EventConfig("signin_from_settings_used", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->blocked_by.type = BlockedBy::Type::NONE;
    return config;
  }

  if (kIPHiOSTabGridSwipeRightForIncognito.name == feature->name) {
    // The IPH of the tab grid swipe feature.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSIncognitoPageControlTapped,
        /*trigger_event=*/"swipe_left_for_incognito_trigger", /*used_event=*/
        feature_engagement::events::
            kIOSSwipeRightForIncognitoUsed, /*dismiss_button_tap_event=*/
        feature_engagement::events::
            kIOSSwipeRightForIncognitoIPHDismissButtonTapped);
  }

  if (kIPHiOSSwipeBackForwardFeature.name == feature->name) {
    // The IPH of the swipe back/forward feature.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSBackForwardButtonTapped,
        /*trigger_event=*/"swipe_back_forward_trigger", /*used_event=*/
        feature_engagement::events::kIOSSwipeBackForwardUsed,
        /*dismiss_button_tap_event=*/
        feature_engagement::events::kIOSSwipeBackForwardIPHDismissButtonTapped);
  }

  if (kIPHiOSSwipeToolbarToChangeTabFeature.name == feature->name) {
    // The IPH of the swipe toolbar to go to adjacent tab feature.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSTabGridAdjacentTabTapped,
        /*trigger_event=*/"swipe_toolbar_to_change_tab_trigger", /*used_event=*/
        feature_engagement::events::kIOSSwipeToolbarToChangeTabUsed,
        /*dismiss_button_tap_event=*/
        feature_engagement::events::
            kIOSSwipeToolbarToChangeTabIPHDismissButtonTapped);
  }

  if (kIPHiOSOverflowMenuCustomizationFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->used = EventConfig(
        feature_engagement::events::kIOSOverflowMenuCustomizationUsed,
        Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    config->trigger =
        EventConfig("overflow_menu_customization_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->event_configs.insert(EventConfig(
        feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed,
        Comparator(GREATER_THAN_OR_EQUAL, 2),
        feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod));
    return config;
  }

  if (kIPHiOSPageInfoRevampFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("IPH_iOSPageInfoRevamp_trigger",
                                  Comparator(LESS_THAN_OR_EQUAL, 3), 365, 365);
    config->used =
        EventConfig("IPH_iOSPageInfoRevamp_used", Comparator(ANY, 0), 365, 365);
    return config;
  }

  if (kIPHiOSInlineEnhancedSafeBrowsingPromoFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(LESS_THAN, 1);
    config->trigger = EventConfig("inline_enhanced_safe_browsing_promo_trigger",
                                  Comparator(LESS_THAN_OR_EQUAL, 10), 360, 360);
    config->event_configs.insert(EventConfig(
        feature_engagement::events::kEnhancedSafeBrowsingPromoCriterionMet,
        Comparator(GREATER_THAN_OR_EQUAL, 1), 7, 360));
    config->event_configs.insert(EventConfig(
        feature_engagement::events::kInlineEnhancedSafeBrowsingPromoClosed,
        Comparator(EQUAL, 0), 360, 360));
    config->used =
        EventConfig("inline_enhanced_safe_browsing_promo_used",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;
    return config;
  }

  // iOS Promo Configs are split out into a separate file, so check that too.
  if (std::optional<FeatureConfig> ios_promo_feature_config =
          GetClientSideiOSPromoFeatureConfig(feature)) {
    return ios_promo_feature_config;
  }

  if (kIPHDiscoverFeedHeaderFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("discover_feed_header_menu_iph_triggered",
                                  Comparator(EQUAL, 0), 365, 365);
    config->used = EventConfig("discover_feed_header_menu_clicked",
                               Comparator(EQUAL, 0), 365, 365);
    return config;
  }

  if (kIPHPlusAddressCreateSuggestionFeature.name == feature->name) {
    // A config that allows a user education bubble to be shown for the plus
    // address feature. Will be shown up to 9 times in the 90 day window with
    // the exception of 2 times if the user accepted the suggestion.

    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("plus_address_create_suggestion_feature_trigger",
                    Comparator(LESS_THAN, 10), 90, 360);
    config->used = EventConfig("plus_address_create_suggestion_feature_used",
                               Comparator(LESS_THAN, 2), 90, 360);
    return config;
  }

  if (kIPHHomeCustomizationMenuFeature.name == feature->name) {
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("home_customization_menu_iph_triggered",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config->used =
        EventConfig(feature_engagement::events::kHomeCustomizationMenuUsed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (std::optional<FeatureConfig> scalable_iph_feature_config =
          GetScalableIphFeatureConfig(feature)) {
    return scalable_iph_feature_config;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (kIPHLauncherSearchHelpUiFeature.name == feature->name) {
    // A config that allows the ChromeOS Ash Launcher search IPH to be shown.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->session_rate_impact.type = SessionRateImpact::Type::NONE;
    config->blocked_by.type = BlockedBy::Type::NONE;
    config->blocking.type = Blocking::Type::NONE;

    // Can be shown any time until the `assistant_click` event is recorded.
    config->trigger =
        EventConfig("IPH_LauncherSearchHelpUi_trigger", Comparator(ANY, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config->used =
        EventConfig("IPH_LauncherSearchHelpUi_chip_click", Comparator(ANY, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config->event_configs.insert(EventConfig(
        "IPH_LauncherSearchHelpUi_assistant_click", Comparator(EQUAL, 0),
        kMaxStoragePeriod, kMaxStoragePeriod));
    return config;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (kIPHDummyFeature.name == feature->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(LESS_THAN, 0);
    config->session_rate = Comparator(LESS_THAN, 0);
    config->trigger = EventConfig("dummy_feature_iph_trigger",
                                  Comparator(LESS_THAN, 0), 1, 1);
    config->used =
        EventConfig("dummy_feature_action", Comparator(LESS_THAN, 0), 1, 1);
    return config;
  }

  return std::nullopt;
}

}  // namespace feature_engagement
