// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/new_tab_page/feature_promo_helper/new_tab_page_feature_promo_helper.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion_handler.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "chrome/grit/new_tab_page_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_scaled_resources.h"
#include "components/history_clusters/core/features.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/image_service_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/core_account_id.h"
#include "media/base/media_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/webui_allowlist.h"
#include "url/origin.h"
#include "url/url_util.h"

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo_handler.h"
#endif

using content::BrowserContext;
using content::WebContents;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabPageUI,
                                      kCustomizeChromeButtonElementId);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabPageUI,
                                      kModulesCustomizeIPHAnchorElement);

bool NewTabPageUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !profile->IsOffTheRecord();
}

std::unique_ptr<content::WebUIController>
NewTabPageUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                          const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUINewTabPageHost);
  }
  return std::make_unique<NewTabPageUI>(web_ui);
}

namespace {

constexpr char kPrevNavigationTimePrefName[] = "NewTabPage.PrevNavigationTime";

bool HasCredentials(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  return
      /* Can be null if Chrome signin is disabled. */ identity_manager &&
      !identity_manager->GetAccountsInCookieJar()
           .GetPotentiallyInvalidSignedInAccounts()
           .empty();
}

content::WebUIDataSource* CreateAndAddNewTabPageUiHtmlSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUINewTabPageHost);

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));
  source->AddString("googleBaseUrl",
                    GURL(TemplateURLServiceFactory::GetForProfile(profile)
                             ->search_terms_data()
                             .GoogleBaseURLValue())
                        .spec());

  source->AddInteger(
      "prerenderStartTimeThreshold",
      features::kNewTabPagePrerenderStartDelayOnMouseHoverByMiliSeconds.Get());
  source->AddInteger(
      "preconnectStartTimeThreshold",
      features::kNewTabPagePreconnectStartDelayOnMouseHoverByMiliSeconds.Get());
  source->AddBoolean(
      "prerenderOnPressEnabled",
      base::FeatureList::IsEnabled(features::kNewTabPageTriggerForPrerender2) &&
          features::kPrerenderNewTabPageOnMousePressedTrigger.Get());
  source->AddBoolean(
      "prerenderOnHoverEnabled",
      base::FeatureList::IsEnabled(features::kNewTabPageTriggerForPrerender2) &&
          features::kPrerenderNewTabPageOnMouseHoverTrigger.Get());

  source->AddBoolean(
      "oneGoogleBarEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpOneGoogleBar));
  source->AddBoolean("shortcutsEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kNtpShortcuts));
  bool redesigned_modules_enabled =
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesRedesigned);
  source->AddBoolean("singleRowShortcutsEnabled", redesigned_modules_enabled);
  source->AddBoolean("logoEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kNtpLogo));
  source->AddBoolean("reducedLogoSpaceEnabled", redesigned_modules_enabled);
  source->AddBoolean(
      "middleSlotPromoEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpMiddleSlotPromo) &&
          profile->GetPrefs()->GetBoolean(prefs::kNtpPromoVisible));
  source->AddBoolean(
      "middleSlotPromoDismissalEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpMiddleSlotPromoDismissal));
  source->AddBoolean("mobilePromoEnabled", base::FeatureList::IsEnabled(
                                               ntp_features::kNtpMobilePromo));
  source->AddBoolean(
      "modulesDragAndDropEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesDragAndDrop));
  source->AddBoolean("modulesLoadEnabled", base::FeatureList::IsEnabled(
                                               ntp_features::kNtpModulesLoad));
  source->AddInteger("modulesLoadTimeout",
                     ntp_features::GetModulesLoadTimeout().InMilliseconds());
  source->AddInteger("modulesMaxColumnCount",
                     ntp_features::GetModulesMaxColumnCount());
  source->AddInteger(
      "multipleLoadedModulesMaxModuleInstanceCount",
      ntp_features::GetMultipleLoadedModulesMaxModuleInstanceCount());
  source->AddBoolean("mostRelevantTabResumptionEnabled",
                     base::FeatureList::IsEnabled(
                         ntp_features::kNtpMostRelevantTabResumptionModule));
  source->AddBoolean(
      "mostRelevantTabResumptionDeviceIconEnabled",
      base::FeatureList::IsEnabled(
          ntp_features::kNtpMostRelevantTabResumptionModuleDeviceIcon));

  static constexpr webui::LocalizedString kStrings[] = {
      {"doneButton", IDS_DONE},
      {"title", IDS_NEW_TAB_TITLE},
      {"undo", IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE},
      {"controlledSettingPolicy", IDS_CONTROLLED_SETTING_POLICY},

      // Custom Links.
      {"addLinkTitle", IDS_NTP_CUSTOM_LINKS_ADD_SHORTCUT_TITLE},
      {"editLinkTitle", IDS_NTP_CUSTOM_LINKS_EDIT_SHORTCUT},
      {"invalidUrl", IDS_NTP_CUSTOM_LINKS_INVALID_URL},
      {"linkAddedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_ADDED},
      {"linkCancel", IDS_NTP_CUSTOM_LINKS_CANCEL},
      {"linkCantCreate", IDS_NTP_CUSTOM_LINKS_CANT_CREATE},
      {"linkCantEdit", IDS_NTP_CUSTOM_LINKS_CANT_EDIT},
      {"linkDone", IDS_NTP_CUSTOM_LINKS_DONE},
      {"linkEditedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_EDITED},
      {"linkRemove", IDS_NTP_CUSTOM_LINKS_REMOVE},
      {"linkRemovedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_REMOVED},
      {"shortcutMoreActions", IDS_NTP_CUSTOM_LINKS_MORE_ACTIONS},
      {"nameField", IDS_NTP_CUSTOM_LINKS_NAME},
      {"restoreDefaultLinks", IDS_NTP_CONFIRM_MSG_RESTORE_DEFAULTS},
      {"restoreThumbnailsShort", IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK},
      {"shortcutAlreadyExists", IDS_NTP_CUSTOM_LINKS_ALREADY_EXISTS},
      {"urlField", IDS_NTP_CUSTOM_LINKS_URL},

      // Customize button and dialog.
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"hueSliderTitle", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_TITLE},
      {"hueSliderAriaLabel", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_ARIA_LABEL},
      {"customizeButton", IDS_NTP_CUSTOMIZE_BUTTON_LABEL},
      {"customizeThisPage", IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL},

      // Wallpaper search.
      {"customizeThisPageWallpaperSearch",
       IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_WALLPAPER_SEARCH_LABEL},
      {"wallpaperSearchButton", IDS_NTP_WALLPAPER_SEARCH_PAGE_HEADER},

      // Voice search.
      // TODO(crbug.com/328827188): Consider moving the voice search overlay
      // code (here and elsewhere) into the searchbox directories or a new
      // component.
      {"audioError", IDS_NEW_TAB_VOICE_AUDIO_ERROR},
      {"close", IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP},
      {"details", IDS_NEW_TAB_VOICE_DETAILS},
      {"languageError", IDS_NEW_TAB_VOICE_LANGUAGE_ERROR},
      {"learnMore", IDS_LEARN_MORE},
      {"learnMoreA11yLabel", IDS_NEW_TAB_VOICE_LEARN_MORE_ACCESSIBILITY_LABEL},
      {"listening", IDS_NEW_TAB_VOICE_LISTENING},
      {"networkError", IDS_NEW_TAB_VOICE_NETWORK_ERROR},
      {"noTranslation", IDS_NEW_TAB_VOICE_NO_TRANSLATION},
      {"noVoice", IDS_NEW_TAB_VOICE_NO_VOICE},
      {"otherError", IDS_NEW_TAB_VOICE_OTHER_ERROR},
      {"permissionError", IDS_NEW_TAB_VOICE_PERMISSION_ERROR},
      {"speak", IDS_NEW_TAB_VOICE_READY},
      {"tryAgain", IDS_NEW_TAB_VOICE_TRY_AGAIN},
      {"waiting", IDS_NEW_TAB_VOICE_WAITING},

      // Lens image search.
      // TODO(crbug.com/328827188): Consider moving the Lens upload dialog code
      // (here and elsewhere) into the searchbox directories or a new component.
      {"lensSearchUploadDialogCloseButtonLabel",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_CLOSE_BUTTON_LABEL},
      {"lensSearchUploadDialogTitle",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_TITLE_SHORT},
      {"lensSearchUploadDialogDragTitle",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_DRAG_TITLE},
      {"lensSearchUploadDialogUploadFileTitle",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_UPLOAD_FILE_TITLE},
      {"lensSearchUploadDialogOrText", IDS_LENS_SEARCH_UPLOAD_DIALOG_OR_TEXT},
      {"lensSearchUploadDialogTextPlaceholder",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_TEXT_PLACEHOLDER},
      {"lensSearchUploadDialogSearchButtonLabel",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_SEARCH_BUTTON_LABEL},
      {"lensSearchUploadDialogDragDropTitle",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_DRAG_DROP_TITLE},
      {"lensSearchUploadDialogLoadingText",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_LOADING_TEXT},
      {"lensSearchUploadDialogOfflineText",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_OFFLINE_TEXT},
      {"lensSearchUploadDialogOfflineSubtitleText",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_OFFLINE_SUBTITLE_TEXT},
      {"lensSearchUploadDialogOfflineButtonLabel",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_OFFLINE_BUTTON_LABEL},
      {"lensSearchUploadDialogErrorFileType",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_ERROR_FILE_TYPE},
      {"lensSearchUploadDialogErrorFileSize",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_ERROR_FILE_SIZE},
      {"lensSearchUploadDialogErrorMultipleFiles",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_ERROR_MULTIPLE_FILES},
      {"lensSearchUploadDialogValidationErrorScheme",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_VALIDATION_ERROR_SCHEME},
      {"lensSearchUploadDialogValidationErrorConformance",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_VALIDATION_ERROR_CONFORMANCE},
      {"lensSearchUploadDialogErrorMultipleUrls",
       IDS_LENS_SEARCH_UPLOAD_DIALOG_ERROR_MULTIPLE_URLS},

      // Logo/doodle.
      {"copyLink", IDS_NTP_DOODLE_SHARE_DIALOG_COPY_LABEL},
      {"doodleLink", IDS_NTP_DOODLE_SHARE_DIALOG_LINK_LABEL},
      {"email", IDS_NTP_DOODLE_SHARE_DIALOG_MAIL_LABEL},
      {"facebook", IDS_NTP_DOODLE_SHARE_DIALOG_FACEBOOK_LABEL},
      {"shareDoodle", IDS_NTP_DOODLE_SHARE_LABEL},
      {"twitter", IDS_NTP_DOODLE_SHARE_DIALOG_TWITTER_LABEL},

      // Theme.
      {"themeCreatedBy", IDS_NEW_TAB_ATTRIBUTION_INTRO},
      {"themeManagedDialogTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      {"themeManagedDialogBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"ok", IDS_OK},

      // Modules.
      {"dismissModuleToastMessage", IDS_NTP_MODULES_DISMISS_TOAST_MESSAGE},
      {"disableModuleToastMessage", IDS_NTP_MODULES_DISABLE_TOAST_MESSAGE},
      {"moduleHeaderMoreActionsMenu", IDS_NTP_MODULE_HEADER_MORE_ACTIONS_MENU},
      {"moduleInfoButtonTitle", IDS_NTP_MODULES_INFO_BUTTON_TITLE},
      {"modulesDismissButtonText", IDS_NTP_MODULES_DISMISS_BUTTON_TEXT},
      {"modulesDisableButtonText", IDS_NTP_MODULES_DISABLE_BUTTON_TEXT},
      {"modulesDisableButtonTextV2", IDS_NTP_MODULES_DISABLE_BUTTON_TEXT_V2},
      {"modulesCustomizeButtonText", IDS_NTP_MODULES_CUSTOMIZE_BUTTON_TEXT},
      {"modulesTasksInfo", IDS_NTP_MODULES_TASKS_INFO},
      {"modulesDisableToastMessage",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_DISABLE_TOAST_MESSAGE},
      {"modulesDriveDisableButtonText",
       IDS_NTP_MODULES_DRIVE_DISABLE_BUTTON_TEXT},
      {"modulesDriveDisableButtonTextV2",
       IDS_NTP_MODULES_DRIVE_DISABLE_BUTTON_TEXT_V2},
      {"modulesDriveDismissButtonText",
       IDS_NTP_MODULES_DRIVE_DISMISS_BUTTON_TEXT},
      {"modulesDriveMoreActionsButtonText",
       IDS_NTP_MODULES_DRIVE_MORE_ACTIONS_BUTTON_TEXT},
      {"modulesDriveSentence", IDS_NTP_MODULES_DRIVE_SENTENCE},
      {"modulesDriveSentence2", IDS_NTP_MODULES_DRIVE_SENTENCE2},
      {"modulesDriveFilesSentence", IDS_NTP_MODULES_DRIVE_FILES_SENTENCE},
      {"modulesDummyLower", IDS_NTP_MODULES_DUMMY_LOWER},
      {"modulesDriveTitle", IDS_NTP_MODULES_DRIVE_TITLE},
      {"modulesDriveTitleV2", IDS_NTP_MODULES_DRIVE_TITLE_V2},
      {"modulesDriveInfo", IDS_NTP_MODULES_DRIVE_INFO},
      {"modulesDummyTitle", IDS_NTP_MODULES_DUMMY_TITLE},
      {"modulesDismissForHoursButtonText",
       IDS_NTP_MODULES_DISMISS_FOR_HOURS_BUTTON_TEXT},
      {"modulesGoogleCalendarDismissToastMessage",
       IDS_NTP_MODULES_GOOGLE_CALENDAR_DISMISS_TOAST_MESSAGE},
      {"modulesGoogleCalendarDisableToastMessage",
       IDS_NTP_MODULES_GOOGLE_CALENDAR_DISABLE_TOAST_MESSAGE},
      {"moduleGoogleCalendarInfo", IDS_NTP_MODULES_GOOGLE_CALENDAR_INFO},
      {"modulesGoogleCalendarMoreActions",
       IDS_NTP_MODULES_GOOGLE_CALENDAR_MORE_ACTIONS},
      {"modulesGoogleCalendarTitle", IDS_NTP_MODULES_GOOGLE_CALENDAR_TITLE},
      {"modulesGoogleCalendarDisableButtonText",
       IDS_NTP_MODULES_GOOGLE_CALENDAR_DISABLE_BUTTON_TEXT},
      {"modulesOutlookCalendarTitle", IDS_NTP_MODULES_OUTLOOK_CALENDAR_TITLE},
      {"modulesOutlookCalendarDisableButtonText",
       IDS_NTP_MODULES_OUTLOOK_CALENDAR_DISABLE_BUTTON_TEXT},
      {"modulesCalendarJoinMeetingButtonText",
       IDS_NTP_MODULES_CALENDAR_JOIN_MEETING_BUTTON_TEXT},
      {"modulesCalendarInProgress", IDS_NTP_MODULES_CALENDAR_IN_PROGRESS},
      {"modulesCalendarInXMin", IDS_NTP_MODULES_CALENDAR_IN_X_MIN},
      {"modulesCalendarInXHr", IDS_NTP_MODULES_CALENDAR_IN_X_HR},
      {"modulesCalendarSeeMore", IDS_NTP_MODULES_CALENDAR_SEE_MORE},
      {"modulesKaleidoscopeTitle", IDS_NTP_MODULES_KALEIDOSCOPE_TITLE},
      {"modulesTasksInfoTitle", IDS_NTP_MODULES_SHOPPING_TASKS_INFO_TITLE},
      {"modulesTasksInfoClose", IDS_NTP_MODULES_SHOPPING_TASKS_INFO_CLOSE},
      {"modulesJourneysShowAll", IDS_NTP_MODULES_SHOW_ALL},
      {"modulesJourneysInfo", IDS_NTP_MODULES_HISTORY_CLUSTERS_INFO},
      {"modulesHistoryDoneButton",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_DONE_BUTTON},
      {"modulesHistoryWithDiscountInfo",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_WITH_DISCOUNT_INFO},
      {"modulesHistoryResumeBrowsingTitle",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_RESUME_BROWSING},
      {"modulesHistoryResumeBrowsingForTitle",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_RESUME_BROWSING_FOR},
      {"modulesThisTypeOfCardText",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_DISABLE_TOAST_NAME},
      {"modulesJourneyDisable",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_DISABLE_DROPDOWN_TEXT},
      {"modulesJourneysDismissButton",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_DISMISS_BUTTON},
      {"modulesJourneysShowAllButton",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_SHOW_ALL_BUTTON},
      {"modulesJourneysShowAllAcc", IDS_ACCNAME_SHOW_ALL},
      {"modulesJourneysSearchSuggAcc", IDS_ACCNAME_SEARCH_SUGG},
      {"modulesJourneysBookmarked",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_BOOKMARKED},
      {"modulesJourneysOpenAllInNewTabGroupButtonText",
       IDS_NTP_MODULES_HISTORY_CLUSTERS_OPEN_ALL_IN_NEW_TAB_GROUP_BUTTON_TEXT},
      {"modulesMoreActions", IDS_NTP_MODULES_MORE_ACTIONS},
      {"modulesTabResumptionDismissButton",
       IDS_NTP_MODULES_TAB_RESUMPTION_DISMISS_BUTTON},
      {"modulesTabResumptionTitle", IDS_NTP_TAB_RESUMPTION_TITLE},
      {"modulesTabResumptionInfo", IDS_NTP_MODULES_TAB_RESUMPTION_INFO},
      {"modulesTabResumptionSentence", IDS_NTP_MODULES_TAB_RESUMPTION_SENTENCE},
      {"modulesTabResumptionDevicePrefix",
       IDS_NTP_MODULES_TAB_RESUMPTION_DEVICE_PREFIX},
      {"modulesMostRelevantTabResumptionDismissAll",
       IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_DISMISS_BUTTON},
      {"modulesMostRelevantTabResumptionTitle",
       IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_TITLE},
      {"modulesMostRelevantTabResumptionSeeMore",
       IDS_NTP_MODULES_MOST_RELEVANT_TAB_RESUMPTION_SEE_MORE},
      {"modulesMostRelevantTabResumptionMostRecent",
       IDS_TAB_RESUME_DECORATORS_MOST_RECENT},
      {"modulesMostRelevantTabResumptionFrequentlyVisited",
       IDS_TAB_RESUME_DECORATORS_FREQUENTLY_VISITED},
      {"modulesMostRelevantTabResumptionVisitedXAgo",
       IDS_TAB_RESUME_DECORATORS_VISITED_X_AGO},

      // Middle slot promo.
      {"undoDismissPromoButtonToast", IDS_NTP_UNDO_DISMISS_PROMO_BUTTON_TOAST},
      {"mobilePromoDescription", IDS_NTP_MOBILE_PROMO_DESCRIPTION},
      {"mobilePromoHeader", IDS_NTP_MOBILE_PROMO_HEADER},

      // Webstore toast.
      {"webstoreThemesToastMessage", IDS_NTP_WEBSTORE_TOAST_MESSAGE},
      {"webstoreThemesToastButtonText", IDS_NTP_WEBSTORE_TOAST_BUTTON_TEXT},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddBoolean("wideModulesEnabled", base::FeatureList::IsEnabled(
                                               ntp_features::kNtpWideModules));

  source->AddBoolean(
      "modulesHeaderIconEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesHeaderIcon));

  source->AddBoolean(
      "modulesOverflowScrollbarEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesOverflowScrollbar));

  source->AddBoolean("modulesRedesignedEnabled", redesigned_modules_enabled);

  source->AddString(
      "calendarModuleDismissHours",
      base::NumberToString(
          ntp_features::kNtpCalendarModuleWindowEndDeltaParam.Get().InHours()));

  SearchboxHandler::SetupWebUIDataSource(
      source, profile,
      /*enable_voice_search=*/true,
      /*enable_lens_search=*/
      profile->GetPrefs()->GetBoolean(prefs::kLensDesktopNTPSearchEnabled));

  webui::SetupWebUIDataSource(
      source, base::make_span(kNewTabPageResources, kNewTabPageResourcesSize),
      IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_HTML);

  // Allow embedding of iframes for the doodle and
  // chrome-untrusted://new-tab-page for other external content and resources.
  // NOTE: Use caution when overriding content security policies as that cean
  // lead to subtle security bugs such as https://crbug.com/1251541.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      base::StringPrintf("child-src https: %s %s;",
                         google_util::CommandLineGoogleBaseURL().spec().c_str(),
                         chrome::kChromeUIUntrustedNewTabPageUrl));

  return source;
}

}  // namespace

NewTabPageUI::NewTabPageUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      content::WebContentsObserver(web_ui->GetWebContents()),
      page_factory_receiver_(this),
      most_visited_page_factory_receiver_(this),
      browser_command_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)),
      tab_(tabs::TabInterface::GetFromContents(web_ui->GetWebContents())),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      ntp_custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile_)),
      // We initialize navigation_start_time_ to a reasonable value to account
      // for the unlikely case where the NewTabPageHandler is created before we
      // received the DidStartNavigation event.
      navigation_start_time_(base::Time::Now()),
      module_id_names_(
          ntp::MakeModuleIdNames(NewTabPageUI::IsManagedProfile(profile_),
                                 profile_)) {
  auto* source = CreateAndAddNewTabPageUiHtmlSource(profile_);
  bool wallpaper_search_button_enabled =
      base::FeatureList::IsEnabled(ntp_features::kNtpWallpaperSearchButton) &&
      customize_chrome::IsWallpaperSearchEnabledForProfile(profile_);
  source->AddBoolean("wallpaperSearchButtonEnabled",
                     wallpaper_search_button_enabled);
  int wallpaper_search_animation_shown_threshold =
      ntp_features::GetWallpaperSearchButtonAnimationShownThreshold();
  // Animate the button if the threshold is negative (unconditional) or if the
  // button has has been shown less times than the threshold.
  bool should_animate_wallpaper_search_button =
      wallpaper_search_animation_shown_threshold < 0 ||
      wallpaper_search_animation_shown_threshold >=
          profile_->GetPrefs()->GetInteger(
              prefs::kNtpWallpaperSearchButtonShownCount);
  source->AddBoolean(
      "wallpaperSearchButtonAnimationEnabled",
      wallpaper_search_button_enabled &&
          base::FeatureList::IsEnabled(
              ntp_features::kNtpWallpaperSearchButtonAnimation) &&
          should_animate_wallpaper_search_button);
  source->AddInteger("wallpaperSearchButtonHideCondition",
                     ntp_features::GetWallpaperSearchButtonHideCondition());

  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
  content::URLDataSource::Add(profile_,
                              std::make_unique<UntrustedSource>(profile_));
  content::URLDataSource::Add(
      profile_,
      std::make_unique<ThemeSource>(profile_, /*serve_untrusted=*/true));

  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

// Give OGB 3P Cookie Permissions. Only necessary on non-Ash builds. Granting
// 3P cookies on Ash causes b/314326552.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  WebUIAllowlist::GetOrCreate(profile_)->RegisterAutoGrantedThirdPartyCookies(
      url::Origin::Create(GURL(chrome::kChromeUIUntrustedNewTabPageUrl)),
      {
          ContentSettingsPattern::FromURL(GURL("https://ogs.google.com")),
          ContentSettingsPattern::FromURL(GURL("https://corp.google.com")),
      });
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ntp_prefs::kNtpUseMostVisitedTiles,
      base::BindRepeating(&NewTabPageUI::OnCustomLinksEnabledPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpShortcutsVisible,
      base::BindRepeating(&NewTabPageUI::OnTilesVisibilityPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Store basic theme info in load time data to make the background color and
  // background image available as soon as the page loads to prevent a potential
  // white flicker.

  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());

  // Populates the load time data with basic info.
  OnColorProviderChanged();
  OnCustomBackgroundImageUpdated();
  OnLoad();

  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &NewTabPageUI::TabWillDetach, weak_ptr_factory_.GetWeakPtr())));
}

WEB_UI_CONTROLLER_TYPE_IMPL(NewTabPageUI)

NewTabPageUI::~NewTabPageUI() {
  // Deregister customize chrome entry on unified side panel, unless the
  // WebContents is showing another NewTabPageUI (e.g. in case of reloads).
  if (auto* web_ui = web_contents()->GetWebUI()) {
    if (web_ui->GetController() && web_ui->GetController()->GetType() &&
        web_ui->GetController()->GetAs<NewTabPageUI>()) {
      return;
    }
  }
}

// static
bool NewTabPageUI::IsNewTabPageOrigin(const GURL& url) {
  return url.DeprecatedGetOriginAsURL() ==
         GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL();
}

// static
void NewTabPageUI::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPrevNavigationTimePrefName, base::Time());
  registry->RegisterBooleanPref(ntp_prefs::kNtpUseMostVisitedTiles, false);
  registry->RegisterBooleanPref(ntp_prefs::kNtpShortcutsVisible, true);
  registry->RegisterBooleanPref(prefs::kNtpPromoVisible, true);
}

// static
void NewTabPageUI::ResetProfilePrefs(PrefService* prefs) {
  ntp_tiles::MostVisitedSites::ResetProfilePrefs(prefs);
  prefs->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, false);
  prefs->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
}

// static
bool NewTabPageUI::IsManagedProfile(Profile* profile) {
  // TODO(crbug.com/40183609): Stop calling the private method
  // FindExtendedPrimaryAccountInfo().
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  return /* Can be null if Chrome signin is disabled. */ identity_manager &&
         identity_manager
             ->FindExtendedPrimaryAccountInfo(signin::ConsentLevel::kSignin)
             .IsManaged();
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  realbox_handler_ = std::make_unique<RealboxHandler>(
      std::move(pending_page_handler), profile_, web_contents(),
      &metrics_reporter_, /*lens_searchbox_client=*/nullptr,
      /*omnibox_controller=*/nullptr);
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  metrics_reporter_.BindInterface(std::move(receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
        pending_receiver) {
  if (browser_command_factory_receiver_.is_bound())
    browser_command_factory_receiver_.reset();
  browser_command_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
        pending_receiver) {
  if (most_visited_page_factory_receiver_.is_bound()) {
    most_visited_page_factory_receiver_.reset();
  }
  most_visited_page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<file_suggestion::mojom::FileSuggestionHandler>
        pending_receiver) {
  file_handler_ = std::make_unique<FileSuggestionHandler>(
      std::move(pending_receiver), profile_);
}

#if !defined(OFFICIAL_BUILD)
void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<foo::mojom::FooHandler> pending_page_handler) {
  foo_handler_ = std::make_unique<FooHandler>(std::move(pending_page_handler));
}
#endif

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp::most_relevant_tab_resumption::mojom::PageHandler>
        pending_page_handler) {
  most_relevant_tab_resumption_handler_ =
      std::make_unique<MostRelevantTabResumptionPageHandler>(
          std::move(pending_page_handler), web_contents());
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp::calendar::mojom::GoogleCalendarPageHandler>
        pending_page_handler) {
  google_calendar_handler_ = std::make_unique<GoogleCalendarPageHandler>(
      std::move(pending_page_handler), profile_);
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<ntp::calendar::mojom::OutlookCalendarPageHandler>
        pending_page_handler) {
  outlook_calendar_handler_ = std::make_unique<OutlookCalendarPageHandler>(
      std::move(pending_page_handler));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
        pending_page_handler) {
  base::WeakPtr<page_image_service::ImageService> image_service_weak;
  if (auto* image_service =
          page_image_service::ImageServiceFactory::GetForBrowserContext(
              profile_)) {
    image_service_weak = image_service->GetWeakPtr();
  }
  image_service_handler_ =
      std::make_unique<page_image_service::ImageServiceHandler>(
          std::move(pending_page_handler), std::move(image_service_weak));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::CreatePageHandler(
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  auto* side_panel_controller =
      tab_->GetTabFeatures()->customize_chrome_side_panel_controller();
  page_handler_ = std::make_unique<NewTabPageHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      ntp_custom_background_service_, theme_service_,
      LogoServiceFactory::GetForProfile(profile_),
      SyncServiceFactory::GetForProfile(profile_),
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile_),
      web_contents(), std::make_unique<NewTabPageFeaturePromoHelper>(),
      navigation_start_time_, &module_id_names_, side_panel_controller);
}

void NewTabPageUI::CreateBrowserCommandHandler(
    mojo::PendingReceiver<browser_command::mojom::CommandHandler>
        pending_handler) {
  using browser_command::mojom::Command;
  std::vector<Command> supported_commands = {
      Command::kOpenSafetyCheck,
      Command::kOpenSafeBrowsingEnhancedProtectionSettings,
      Command::kNoOpCommand,
  };
  promo_browser_command_handler_ = std::make_unique<BrowserCommandHandler>(
      std::move(pending_handler), profile_, supported_commands);
}

void NewTabPageUI::CreatePageHandler(
    mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  most_visited_page_handler_ = std::make_unique<MostVisitedHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents(), GURL(chrome::kChromeUINewTabPageURL),
      navigation_start_time_);
  most_visited_page_handler_->EnableCustomLinks(IsCustomLinksEnabled());
  most_visited_page_handler_->SetShortcutsVisible(IsShortcutsVisible());
}

void NewTabPageUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{
          NewTabPageUI::kCustomizeChromeButtonElementId,
          NewTabPageUI::kModulesCustomizeIPHAnchorElement});
}

// OnColorProviderChanged can be called during the destruction process and
// should not directly access any member variables.
void NewTabPageUI::OnColorProviderChanged() {
  base::Value::Dict update;
  if (!web_contents() || !web_ui())
    return;
  const ui::ColorProvider& color_provider = web_contents()->GetColorProvider();
  auto background_color = color_provider.GetColor(kColorNewTabPageBackground);
  update.Set("backgroundColor", skia::SkColorToHexString(background_color));
  content::WebUIDataSource::Update(Profile::FromWebUI(web_ui()),
                                   chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}

void NewTabPageUI::OnCustomBackgroundImageUpdated() {
  base::Value::Dict update;
  url::RawCanonOutputT<char> encoded_url;
  auto custom_background_url =
      (ntp_custom_background_service_
           ? ntp_custom_background_service_->GetCustomBackground()
           : std::optional<CustomBackground>())
          .value_or(CustomBackground())
          .custom_background_url;
  url::EncodeURIComponent(custom_background_url.spec(), &encoded_url);
  update.Set(
      "backgroundImageUrl",
      encoded_url.length() > 0
          ? base::StrCat(
                {"chrome-untrusted://new-tab-page/custom_background_image?url=",
                 encoded_url.view()})
          : "");
  content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}

void NewTabPageUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->GetURL() == GURL(chrome::kChromeUINewTabPageURL)) {
    navigation_start_time_ = base::Time::Now();

    OnLoad();
    auto prev_navigation_time =
        profile_->GetPrefs()->GetTime(kPrevNavigationTimePrefName);
    if (!prev_navigation_time.is_null()) {
      base::UmaHistogramCustomTimes(
          "NewTabPage.TimeSinceLastNTP",
          navigation_start_time_ - prev_navigation_time, base::Seconds(1),
          base::Days(1), 100);
    }
    profile_->GetPrefs()->SetTime(kPrevNavigationTimePrefName,
                                  navigation_start_time_);
    base::UmaHistogramBoolean("NewTabPage.HasCredentials",
                              HasCredentials(profile_));
  }
}

bool NewTabPageUI::IsCustomLinksEnabled() const {
  return !profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles);
}

bool NewTabPageUI::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

void NewTabPageUI::OnCustomLinksEnabledPrefChanged() {
  if (most_visited_page_handler_) {
    most_visited_page_handler_->EnableCustomLinks(IsCustomLinksEnabled());
  }
}

void NewTabPageUI::OnTilesVisibilityPrefChanged() {
  if (most_visited_page_handler_) {
    most_visited_page_handler_->SetShortcutsVisible(IsShortcutsVisible());
  }
}

void NewTabPageUI::OnLoad() {
  base::Value::Dict update;
  update.Set("navigationStartTime",
             navigation_start_time_.InMillisecondsFSinceUnixEpoch());
  update.Set(
      "modulesEnabled",
      ntp::HasModulesEnabled(module_id_names_,
                             IdentityManagerFactory::GetForProfile(profile_)));
  content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}

void NewTabPageUI::TabWillDetach(tabs::TabInterface* tab,
                                 tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    tab_ = nullptr;
    if (page_handler_) {
      page_handler_->TabWillDelete();
    }
  }
}

// static
base::RefCountedMemory* NewTabPageUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_NTP_FAVICON, scale_factor));
}
