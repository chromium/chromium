/// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_configurations.h"

#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

absl::optional<FeatureConfig> GetClientSideFeatureConfig(
    const base::Feature* feature) {
#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
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
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)

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
  if (kIPHAddToHomescreenTextBubbleFeature.name == feature->name) {
    // A config that allows the Add to homescreen text bubble IPH to be shown:
    // * Once per 15 days
    // * Up to 2 times but only if unused in the last 15 days.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("add_to_homescreen_text_bubble_iph_trigger",
                                  Comparator(LESS_THAN, 2), 90, 90);
    config->used = EventConfig("add_to_homescreen_dialog_shown",
                               Comparator(EQUAL, 0), 90, 90);
    config->event_configs.insert(
        EventConfig("add_to_homescreen_text_bubble_iph_trigger",
                    Comparator(EQUAL, 0), 15, 90));
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
    config->used = EventConfig(
        "feature_notification_guide_incognito_tab_notification_used",
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
    config->used =
        EventConfig("feature_notification_guide_voice_search_notification_used",
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

  if (kIPHStartSurfaceTabSwitcherHomeButton.name == feature->name) {
    // A config that allows the StartSurfaceTabSwitcherHomeButton IPH to be
    // shown:
    // * Once per day
    // * Up to 7 times but only if the home button is not clicked when IPH is
    // showing.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("start_surface_tab_switcher_home_button_iph_trigger",
                    Comparator(LESS_THAN, 7), k10YearsInDays, k10YearsInDays);
    config->used =
        EventConfig("start_surface_tab_switcher_home_button_clicked",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    config->event_configs.insert(
        EventConfig("start_surface_tab_switcher_home_button_iph_trigger",
                    Comparator(EQUAL, 0), 1, 360));
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

  if (kIPHKeyboardAccessoryPaymentVirtualCardFeature.name == feature->name) {
    // A config that allows the virtual card IPH to be shown when a user
    // interacts with the payment form and triggers the credit card suggestion
    // list.
    absl::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(ANY, 0);
    config->trigger =
        EventConfig("keyboard_accessory_payment_virtual_card_iph_trigger",
                    Comparator(LESS_THAN, 3), 90, 360);
    config->used = EventConfig("keyboard_accessory_payment_suggestion_accepted",
                               Comparator(LESS_THAN, 2), 90, 360);

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::EXPLICIT;
    std::vector<std::string> affected_features;
    affected_features.push_back("IPH_KeyboardAccessoryBarSwiping");
    session_rate_impact.affected_features = affected_features;
    config->session_rate_impact = session_rate_impact;
    return config;
  }

#endif  // defined(OS_ANDROID)

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
