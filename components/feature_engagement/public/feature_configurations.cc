// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_configurations.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"

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

  if (kIPHIntentChipFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);

    // Show the IPH once a month if the intent chip hasn't opened any app in
    // a year.
    config->trigger =
        EventConfig("intent_chip_trigger", Comparator(EQUAL, 0), 30, 360);
    config->used =
        EventConfig("intent_chip_opened_app", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHBatterySaverModeFeature.name == feature->name) {
    // Show promo once a year when the battery saver toolbar icon is visible.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger = EventConfig("battery_saver_info_triggered",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used =
        EventConfig("battery_saver_info_shown", Comparator(EQUAL, 0), 7, 360);
    return config;
  }

  if (kIPHHighEfficiencyInfoModeFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the page action chip was not opened
    // within the last week
    config->trigger = EventConfig("high_efficiency_info_trigger",
                                  Comparator(EQUAL, 0), 360, 360);
    config->used =
        EventConfig("high_efficiency_info_shown", Comparator(EQUAL, 0), 7, 360);
    return config;
  }

  if (kIPHHighEfficiencyModeFeature.name == feature->name) {
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    // Show the promo max 3 times, once per day.
    config->trigger = EventConfig("high_efficiency_prompt_in_trigger",
                                  Comparator(LESS_THAN, 1), 1, 360);
    // This event is never logged but is included for consistency.
    config->used = EventConfig("high_efficiency_prompt_in_used",
                               Comparator(EQUAL, 0), 360, 360);
    config->event_configs.insert(
        EventConfig("high_efficiency_prompt_in_trigger",
                    Comparator(LESS_THAN, 3), 360, 360));
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

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)

  constexpr int k10YearsInDays = 365 * 10;

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
  if (kIPHExploreSitesTileFeature.name == feature->name) {
    // A config that allows the ExploreSites IPH to be shown:
    // * Once per day
    // * Up to 3 times but only if unused in the last 90 days.
    absl::optional<FeatureConfig> config = FeatureConfig();
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
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
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
    // year.

    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("whats_new_trigger", Comparator(EQUAL, 0), 360, 360);
    config->used =
        EventConfig("whats_new_used", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHPriceNotificationsWhileBrowsingFeature.name == feature->name) {
    // A config that allows a user education bubble to be shown for the bottom
    // toolbar.

    // TODO(crbug.com/1382913): Set the trigger policy to the desired occurrence
    // frequency threshold. Currently, the threshold is set to an arbitrary
    // value.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger =
        EventConfig("price_notifications_trigger", Comparator(EQUAL, 0), 7, 7);
    config->used =
        EventConfig("price_notifications_used", Comparator(EQUAL, 0), 7, 7);
    return config;
  }
#endif  // BUILDFLAG(IS_IOS)

  if (kIPHDummyFeature.name == feature->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = false;
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
