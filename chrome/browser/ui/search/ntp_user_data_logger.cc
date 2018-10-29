// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/ntp_user_data_logger.h"

#include <algorithm>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/ntp_user_data_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/ntp_tiles/metrics.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace {

// This enum must match the numbering for NewTabPageVoiceAction in enums.xml.
// Do not reorder or remove items, only add new items before VOICE_ACTION_MAX.
enum VoiceAction {
  // Activated by clicking on the fakebox icon.
  VOICE_ACTION_ACTIVATE_FAKEBOX = 0,
  // Activated by keyboard shortcut.
  VOICE_ACTION_ACTIVATE_KEYBOARD = 1,
  // Close the voice overlay by a user's explicit action.
  VOICE_ACTION_CLOSE_OVERLAY = 2,
  // Submitted voice query.
  VOICE_ACTION_QUERY_SUBMITTED = 3,
  // Clicked on support link in error message.
  VOICE_ACTION_SUPPORT_LINK_CLICKED = 4,
  // Retried by clicking Try Again link.
  VOICE_ACTION_TRY_AGAIN_LINK = 5,
  // Retried by clicking microphone button.
  VOICE_ACTION_TRY_AGAIN_MIC_BUTTON = 6,

  VOICE_ACTION_MAX
};

// Converts |NTPLoggingEventType| to a |VoiceAction|, if the value
// is an action value. Otherwise, |VOICE_ACTION_MAX| is returned.
VoiceAction LoggingEventToVoiceAction(NTPLoggingEventType event) {
  switch (event) {
    case NTP_VOICE_ACTION_ACTIVATE_FAKEBOX:
      return VOICE_ACTION_ACTIVATE_FAKEBOX;
    case NTP_VOICE_ACTION_ACTIVATE_KEYBOARD:
      return VOICE_ACTION_ACTIVATE_KEYBOARD;
    case NTP_VOICE_ACTION_CLOSE_OVERLAY:
      return VOICE_ACTION_CLOSE_OVERLAY;
    case NTP_VOICE_ACTION_QUERY_SUBMITTED:
      return VOICE_ACTION_QUERY_SUBMITTED;
    case NTP_VOICE_ACTION_SUPPORT_LINK_CLICKED:
      return VOICE_ACTION_SUPPORT_LINK_CLICKED;
    case NTP_VOICE_ACTION_TRY_AGAIN_LINK:
      return VOICE_ACTION_TRY_AGAIN_LINK;
    case NTP_VOICE_ACTION_TRY_AGAIN_MIC_BUTTON:
      return VOICE_ACTION_TRY_AGAIN_MIC_BUTTON;
    default:
      NOTREACHED();
      return VOICE_ACTION_MAX;
  }
}

// This enum must match the numbering for NewTabPageVoiceError in enums.xml.
// Do not reorder or remove items, only add new items before VOICE_ERROR_MAX.
enum VoiceError {
  VOICE_ERROR_ABORTED = 0,
  VOICE_ERROR_AUDIO_CAPTURE = 1,
  VOICE_ERROR_BAD_GRAMMAR = 2,
  VOICE_ERROR_LANGUAGE_NOT_SUPPORTED = 3,
  VOICE_ERROR_NETWORK = 4,
  VOICE_ERROR_NO_MATCH = 5,
  VOICE_ERROR_NO_SPEECH = 6,
  VOICE_ERROR_NOT_ALLOWED = 7,
  VOICE_ERROR_OTHER = 8,
  VOICE_ERROR_SERVICE_NOT_ALLOWED = 9,

  VOICE_ERROR_MAX
};

// Key used in prefs::kNtpCustomBackgroundDict to save a background image URL.
// TODO(crbug.com/873699): Refactor customization check for better testability.
const char kNtpCustomBackgroundURL[] = "background_url";

// Logs BackgroundCustomization availability on the NTP,
void LogBackgroundCustomizationAvailability(
    BackgroundCustomization availability) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.CustomizationAvailability.Backgrounds",
                            availability);
}

// Logs ShortcutCustomization availability on the NTP,
void LogShortcutCustomizationAvailability(ShortcutCustomization availability) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.CustomizationAvailability.Shortcuts",
                            availability);
}

// Converts |NTPLoggingEventType| to a |CustomizedFeature|.
CustomizedFeature LoggingEventToCustomizedFeature(NTPLoggingEventType event) {
  switch (event) {
    case NTP_BACKGROUND_CUSTOMIZED:
      return CustomizedFeature::CUSTOMIZED_FEATURE_BACKGROUND;
    case NTP_SHORTCUT_CUSTOMIZED:
      return CustomizedFeature::CUSTOMIZED_FEATURE_SHORTCUT;
    default:
      break;
  }

  NOTREACHED();
  return CustomizedFeature::CUSTOMIZED_FEATURE_BACKGROUND;
}

// Converts |NTPLoggingEventType| to a |CustomizeAction|.
CustomizeAction LoggingEventToCustomizeAction(NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED:
      return CustomizeAction::CUSTOMIZE_ACTION_CHROME_BACKGROUNDS;
    case NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED:
      return CustomizeAction::CUSTOMIZE_ACTION_LOCAL_IMAGE;
    case NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED:
      return CustomizeAction::CUSTOMIZE_ACTION_RESTORE_BACKGROUND;
    case NTP_CUSTOMIZE_ATTRIBUTION_CLICKED:
      return CustomizeAction::CUSTOMIZE_ACTION_ATTRIBUTION;
    case NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED:
      return CustomizeAction::CUSTOMIZE_ACTION_ADD_SHORTCUT;
    case NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED:
      return CustomizeAction::CUSTOMIZE_ACTION_EDIT_SHORTCUT;
    case NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED:
      return CustomizeAction::CUSTOMIZE_ACTION_RESTORE_SHORTCUT;
    default:
      break;
  }

  NOTREACHED();
  return CustomizeAction::CUSTOMIZE_ACTION_CHROME_BACKGROUNDS;
}

// Converts |NTPLoggingEventType| to a |CustomizeChromeBackgroundAction|.
CustomizeChromeBackgroundAction LoggingEventToCustomizeChromeBackgroundAction(
    NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_COLLECTION;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_IMAGE;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_CANCEL;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE:
      return CustomizeChromeBackgroundAction::
          CUSTOMIZE_CHROME_BACKGROUND_ACTION_DONE;
    default:
      break;
  }

  NOTREACHED();
  return CustomizeChromeBackgroundAction::
      CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_COLLECTION;
}

// Converts |NTPLoggingEventType| to a |CustomizeLocalImageBackgroundAction|.
CustomizeLocalImageBackgroundAction
LoggingEventToCustomizeLocalImageBackgroundAction(NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL:
      return CustomizeLocalImageBackgroundAction::
          CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_CANCEL;
    case NTP_CUSTOMIZE_LOCAL_IMAGE_DONE:
      return CustomizeLocalImageBackgroundAction::
          CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_DONE;
    default:
      break;
  }

  NOTREACHED();
  return CustomizeLocalImageBackgroundAction::
      CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_CANCEL;
}

// Converts |NTPLoggingEventType| to a |CustomizeShortcutAction|.
CustomizeShortcutAction LoggingEventToCustomizeShortcutAction(
    NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_SHORTCUT_ADD:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_ADD;
    case NTP_CUSTOMIZE_SHORTCUT_UPDATE:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_UPDATE;
    case NTP_CUSTOMIZE_SHORTCUT_REMOVE:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_REMOVE;
    case NTP_CUSTOMIZE_SHORTCUT_CANCEL:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_CANCEL;
    case NTP_CUSTOMIZE_SHORTCUT_DONE:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_DONE;
    case NTP_CUSTOMIZE_SHORTCUT_UNDO:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_UNDO;
    case NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL:
      return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_RESTORE_ALL;
    default:
      break;
  }

  NOTREACHED();
  return CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_REMOVE;
}

// Converts |NTPLoggingEventType| to a |VoiceError|, if the value
// is an error value. Otherwise, |VOICE_ERROR_MAX| is returned.
VoiceError LoggingEventToVoiceError(NTPLoggingEventType event) {
  switch (event) {
    case NTP_VOICE_ERROR_ABORTED:
      return VOICE_ERROR_ABORTED;
    case NTP_VOICE_ERROR_AUDIO_CAPTURE:
      return VOICE_ERROR_AUDIO_CAPTURE;
    case NTP_VOICE_ERROR_BAD_GRAMMAR:
      return VOICE_ERROR_BAD_GRAMMAR;
    case NTP_VOICE_ERROR_LANGUAGE_NOT_SUPPORTED:
      return VOICE_ERROR_LANGUAGE_NOT_SUPPORTED;
    case NTP_VOICE_ERROR_NETWORK:
      return VOICE_ERROR_NETWORK;
    case NTP_VOICE_ERROR_NO_MATCH:
      return VOICE_ERROR_NO_MATCH;
    case NTP_VOICE_ERROR_NO_SPEECH:
      return VOICE_ERROR_NO_SPEECH;
    case NTP_VOICE_ERROR_NOT_ALLOWED:
      return VOICE_ERROR_NOT_ALLOWED;
    case NTP_VOICE_ERROR_OTHER:
      return VOICE_ERROR_OTHER;
    case NTP_VOICE_ERROR_SERVICE_NOT_ALLOWED:
      return VOICE_ERROR_SERVICE_NOT_ALLOWED;
    default:
      NOTREACHED();
      return VOICE_ERROR_MAX;
  }
}

// This enum must match the numbering for NewTabPageLogoShown in enums.xml.
// Do not reorder or remove items, and only add new items before
// LOGO_IMPRESSION_TYPE_MAX.
enum LogoImpressionType {
  // Static Doodle image.
  LOGO_IMPRESSION_TYPE_STATIC = 0,
  // Call-to-action Doodle image.
  LOGO_IMPRESSION_TYPE_CTA = 1,

  LOGO_IMPRESSION_TYPE_MAX
};

// This enum must match the numbering for NewTabPageLogoClick in enums.xml.
// Do not reorder or remove items, and only add new items before
// LOGO_CLICK_TYPE_MAX.
enum LogoClickType {
  // Static Doodle image.
  LOGO_CLICK_TYPE_STATIC = 0,
  // Call-to-action Doodle image.
  LOGO_CLICK_TYPE_CTA = 1,
  // Animated Doodle image.
  LOGO_CLICK_TYPE_ANIMATED = 2,

  LOGO_CLICK_TYPE_MAX
};

// Converts |NTPLoggingEventType| to a |LogoClickType|, if the value
// is an error value. Otherwise, |LOGO_CLICK_TYPE_MAX| is returned.
LogoClickType LoggingEventToLogoClick(NTPLoggingEventType event) {
  switch (event) {
    case NTP_STATIC_LOGO_CLICKED:
      return LOGO_CLICK_TYPE_STATIC;
    case NTP_CTA_LOGO_CLICKED:
      return LOGO_CLICK_TYPE_CTA;
    case NTP_ANIMATED_LOGO_CLICKED:
      return LOGO_CLICK_TYPE_ANIMATED;
    default:
      NOTREACHED();
      return LOGO_CLICK_TYPE_MAX;
  }
}

}  // namespace

// Helper macro to log a load time to UMA. There's no good reason why we don't
// use one of the standard UMA_HISTORAM_*_TIMES macros, but all their ranges are
// different, and it's not worth changing all the existing histograms.
#define UMA_HISTOGRAM_LOAD_TIME(name, sample)                      \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                         \
                             base::TimeDelta::FromMilliseconds(1), \
                             base::TimeDelta::FromSeconds(60), 100)

NTPUserDataLogger::~NTPUserDataLogger() {}

// static
NTPUserDataLogger* NTPUserDataLogger::GetOrCreateFromWebContents(
    content::WebContents* content) {
  DCHECK(search::IsInstantNTP(content));

  // Calling CreateForWebContents when an instance is already attached has no
  // effect, so we can do this.
  NTPUserDataLogger::CreateForWebContents(content);
  NTPUserDataLogger* logger = NTPUserDataLogger::FromWebContents(content);

  // We record the URL of this NTP in order to identify navigations that
  // originate from it. We use the NavigationController's URL since it might
  // differ from the WebContents URL which is usually chrome://newtab/.
  //
  // We update the NTP URL every time this function is called, because the NTP
  // URL sometimes changes while it is open, and we care about the final one for
  // detecting when the user leaves or returns to the NTP. In particular, if the
  // Google URL changes (e.g. google.com -> google.de), then we fall back to the
  // local NTP.
  const content::NavigationEntry* entry =
      content->GetController().GetVisibleEntry();
  if (entry && (logger->ntp_url_ != entry->GetURL())) {
    DVLOG(1) << "NTP URL changed from \"" << logger->ntp_url_ << "\" to \""
             << entry->GetURL() << "\"";
    logger->ntp_url_ = entry->GetURL();
  }

  logger->profile_ = Profile::FromBrowserContext(content->GetBrowserContext());
  return logger;
}

void NTPUserDataLogger::LogEvent(NTPLoggingEventType event,
                                 base::TimeDelta time) {
  if (event == NTP_ALL_TILES_LOADED) {
    EmitNtpStatistics(time);
  }

  // All other events can only be logged by the Google NTP
  if (!DefaultSearchProviderIsGoogle()) {
    return;
  }

  switch (event) {
    case NTP_ALL_TILES_RECEIVED:
      tiles_received_time_ = time;
      break;
    case NTP_ALL_TILES_LOADED:
      // permitted above for non-Google search providers
      break;
    case NTP_VOICE_ACTION_ACTIVATE_FAKEBOX:
    case NTP_VOICE_ACTION_ACTIVATE_KEYBOARD:
    case NTP_VOICE_ACTION_CLOSE_OVERLAY:
    case NTP_VOICE_ACTION_QUERY_SUBMITTED:
    case NTP_VOICE_ACTION_SUPPORT_LINK_CLICKED:
    case NTP_VOICE_ACTION_TRY_AGAIN_LINK:
    case NTP_VOICE_ACTION_TRY_AGAIN_MIC_BUTTON:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.VoiceActions",
                                LoggingEventToVoiceAction(event),
                                VOICE_ACTION_MAX);
      break;
    case NTP_VOICE_ERROR_ABORTED:
    case NTP_VOICE_ERROR_AUDIO_CAPTURE:
    case NTP_VOICE_ERROR_BAD_GRAMMAR:
    case NTP_VOICE_ERROR_LANGUAGE_NOT_SUPPORTED:
    case NTP_VOICE_ERROR_NETWORK:
    case NTP_VOICE_ERROR_NO_MATCH:
    case NTP_VOICE_ERROR_NO_SPEECH:
    case NTP_VOICE_ERROR_NOT_ALLOWED:
    case NTP_VOICE_ERROR_OTHER:
    case NTP_VOICE_ERROR_SERVICE_NOT_ALLOWED:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.VoiceErrors",
                                LoggingEventToVoiceError(event),
                                VOICE_ERROR_MAX);
      break;
    case NTP_STATIC_LOGO_SHOWN_FROM_CACHE:
      RecordDoodleImpression(time, /*is_cta=*/false, /*from_cache=*/true);
      break;
    case NTP_STATIC_LOGO_SHOWN_FRESH:
      RecordDoodleImpression(time, /*is_cta=*/false, /*from_cache=*/false);
      break;
    case NTP_CTA_LOGO_SHOWN_FROM_CACHE:
      RecordDoodleImpression(time, /*is_cta=*/true, /*from_cache=*/true);
      break;
    case NTP_CTA_LOGO_SHOWN_FRESH:
      RecordDoodleImpression(time, /*is_cta=*/true, /*from_cache=*/false);
      break;
    case NTP_STATIC_LOGO_CLICKED:
    case NTP_CTA_LOGO_CLICKED:
    case NTP_ANIMATED_LOGO_CLICKED:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoClick",
                                LoggingEventToLogoClick(event),
                                LOGO_CLICK_TYPE_MAX);
      break;
    case NTP_ONE_GOOGLE_BAR_SHOWN:
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.OneGoogleBar.ShownTime", time);
      break;
    case NTP_BACKGROUND_CUSTOMIZED:
    case NTP_SHORTCUT_CUSTOMIZED:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.Customized",
                                LoggingEventToCustomizedFeature(event));
      break;
    case NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED:
    case NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED:
    case NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED:
    case NTP_CUSTOMIZE_ATTRIBUTION_CLICKED:
    case NTP_CUSTOMIZE_ADD_SHORTCUT_CLICKED:
    case NTP_CUSTOMIZE_EDIT_SHORTCUT_CLICKED:
    case NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.CustomizeAction",
                                LoggingEventToCustomizeAction(event));
      break;
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION:
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE:
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL:
    case NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeChromeBackgroundAction",
          LoggingEventToCustomizeChromeBackgroundAction(event));
      break;
    case NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL:
    case NTP_CUSTOMIZE_LOCAL_IMAGE_DONE:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeLocalImageBackgroundAction",
          LoggingEventToCustomizeLocalImageBackgroundAction(event));
      break;
    case NTP_CUSTOMIZE_SHORTCUT_ADD:
    case NTP_CUSTOMIZE_SHORTCUT_UPDATE:
    case NTP_CUSTOMIZE_SHORTCUT_REMOVE:
    case NTP_CUSTOMIZE_SHORTCUT_CANCEL:
    case NTP_CUSTOMIZE_SHORTCUT_DONE:
    case NTP_CUSTOMIZE_SHORTCUT_UNDO:
    case NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL:
      UMA_HISTOGRAM_ENUMERATION("NewTabPage.CustomizeShortcutAction",
                                LoggingEventToCustomizeShortcutAction(event));
      break;
  }
}

void NTPUserDataLogger::LogMostVisitedImpression(
    const ntp_tiles::NTPTileImpression& impression) {
  if ((impression.index >= kNumMostVisited) ||
      logged_impressions_[impression.index].has_value()) {
    return;
  }
  logged_impressions_[impression.index] = impression;
}

void NTPUserDataLogger::LogMostVisitedNavigation(
    const ntp_tiles::NTPTileImpression& impression) {
  ntp_tiles::metrics::RecordTileClick(impression);

  // Records the action. This will be available as a time-stamped stream
  // server-side and can be used to compute time-to-long-dwell.
  base::RecordAction(base::UserMetricsAction("MostVisited_Clicked"));
}

NTPUserDataLogger::NTPUserDataLogger(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      has_emitted_(false),
      should_record_doodle_load_time_(true),
      during_startup_(!AfterStartupTaskUtils::IsBrowserStartupComplete()) {
}

// content::WebContentsObserver override
void NTPUserDataLogger::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  NavigatedFromURLToURL(load_details.previous_url,
                        load_details.entry->GetURL());
}

void NTPUserDataLogger::NavigatedFromURLToURL(const GURL& from,
                                              const GURL& to) {
  // User is returning to NTP, probably via the back button; reset stats.
  if (from.is_valid() && to.is_valid() && (to == ntp_url_)) {
    DVLOG(1) << "Returning to New Tab Page";
    logged_impressions_.fill(base::nullopt);
    tiles_received_time_ = base::TimeDelta();
    has_emitted_ = false;
    should_record_doodle_load_time_ = true;
  }
}

bool NTPUserDataLogger::DefaultSearchProviderIsGoogle() const {
  return search::DefaultSearchProviderIsGoogle(profile_);
}

bool NTPUserDataLogger::ThemeIsConfigured() const {
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  return !theme_service->GetThemeID().empty();
}

bool NTPUserDataLogger::CustomBackgroundIsConfigured() const {
  const base::DictionaryValue* background_info =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpCustomBackgroundDict);
  const base::Value* background_url =
      background_info->FindKey(kNtpCustomBackgroundURL);
  if (!background_url) {
    return false;
  }
  GURL custom_background_url(background_url->GetString());
  return custom_background_url.is_valid();
}

void NTPUserDataLogger::EmitNtpStatistics(base::TimeDelta load_time) {
  // We only send statistics once per page.
  if (has_emitted_) {
    return;
  }

  bool has_server_side_suggestions = false;
  int tiles_count = 0;
  for (const base::Optional<ntp_tiles::NTPTileImpression>& impression :
       logged_impressions_) {
    if (!impression.has_value()) {
      break;
    }
    if (impression->source == ntp_tiles::TileSource::SUGGESTIONS_SERVICE) {
      has_server_side_suggestions = true;
    }
    // No URL and rappor service passed - not interested in favicon-related
    // Rappor metrics.
    ntp_tiles::metrics::RecordTileImpression(*impression,
                                             /*rappor_service=*/nullptr);
    ++tiles_count;
  }
  ntp_tiles::metrics::RecordPageImpression(tiles_count);

  DVLOG(1) << "Emitting NTP load time: " << load_time << ", "
           << "number of tiles: " << tiles_count;

  UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime", tiles_received_time_);
  UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime", load_time);

  // Split between ML (aka SuggestionsService) and MV (aka TopSites).
  if (has_server_side_suggestions) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.MostLikely",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.MostLikely", load_time);
  } else {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.MostVisited",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.MostVisited", load_time);
  }

  // Note: This could be inaccurate if the default search engine was changed
  // since the page load started. That's unlikely enough to not warrant special
  // handling.
  bool is_google = DefaultSearchProviderIsGoogle();
  bool is_theme_configured = ThemeIsConfigured();

  // Split between Web and Local.
  if (ntp_url_.SchemeIsHTTPOrHTTPS()) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.Web",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.Web", load_time);
    // Further split between Google and non-Google.
    if (is_google) {
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.Web.Google", load_time);
    } else {
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.Web.Other", load_time);
    }
  } else {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.LocalNTP",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.LocalNTP", load_time);
    // Further split between Google and non-Google.
    if (is_google) {
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.LocalNTP.Google", load_time);
    } else {
      UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.LocalNTP.Other", load_time);
    }
  }

  // Split between Startup and non-startup.
  if (during_startup_) {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.Startup",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.Startup", load_time);
  } else {
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.TilesReceivedTime.NewTab",
                            tiles_received_time_);
    UMA_HISTOGRAM_LOAD_TIME("NewTabPage.LoadTime.NewTab", load_time);
  }

  if (features::IsCustomBackgroundsEnabled()) {
    if (!is_google) {
      // TODO(crbug.com/869931): This is only emitted upon search engine change.
      LogBackgroundCustomizationAvailability(
          BackgroundCustomization::
              BACKGROUND_CUSTOMIZATION_UNAVAILABLE_SEARCH_PROVIDER);
    } else if (is_theme_configured) {
      LogBackgroundCustomizationAvailability(
          BackgroundCustomization::BACKGROUND_CUSTOMIZATION_UNAVAILABLE_THEME);
    } else {
      LogBackgroundCustomizationAvailability(
          BackgroundCustomization::BACKGROUND_CUSTOMIZATION_AVAILABLE);
    }
  } else {
    LogBackgroundCustomizationAvailability(
        BackgroundCustomization::BACKGROUND_CUSTOMIZATION_UNAVAILABLE_FEATURE);
  }

  if (features::IsCustomLinksEnabled()) {
    if (!is_google) {
      // TODO(crbug.com/869931): This is only emitted upon search engine change.
      LogShortcutCustomizationAvailability(
          ShortcutCustomization::
              SHORTCUT_CUSTOMIZATION_UNAVAILABLE_SEARCH_PROVIDER);
    } else {
      LogShortcutCustomizationAvailability(
          ShortcutCustomization::SHORTCUT_CUSTOMIZATION_AVAILABLE);
    }
  } else {
    LogShortcutCustomizationAvailability(
        ShortcutCustomization::SHORTCUT_CUSTOMIZATION_UNAVAILABLE_FEATURE);
  }

  if (CustomBackgroundIsConfigured()) {
    UMA_HISTOGRAM_ENUMERATION(
        "NewTabPage.Customized",
        LoggingEventToCustomizedFeature(NTP_BACKGROUND_CUSTOMIZED));
  }
  has_emitted_ = true;
  during_startup_ = false;
}

void NTPUserDataLogger::RecordDoodleImpression(base::TimeDelta time,
                                               bool is_cta,
                                               bool from_cache) {
  LogoImpressionType logo_type =
      is_cta ? LOGO_IMPRESSION_TYPE_CTA : LOGO_IMPRESSION_TYPE_STATIC;
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoShown", logo_type,
                            LOGO_IMPRESSION_TYPE_MAX);
  if (from_cache) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoShown.FromCache", logo_type,
                              LOGO_IMPRESSION_TYPE_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.LogoShown.Fresh", logo_type,
                              LOGO_IMPRESSION_TYPE_MAX);
  }

  if (should_record_doodle_load_time_) {
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.LogoShownTime2", time);
    should_record_doodle_load_time_ = false;
  }
}
