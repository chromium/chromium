// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/preloading/autocomplete_dictionary_preload_service.h"
#include "chrome/browser/preloading/autocomplete_dictionary_preload_service_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/search_preload/search_preload_service.h"
#include "chrome/browser/preloading/search_preload/search_preload_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_client.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "third_party/omnibox_proto/answer_data.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

namespace searchbox_internal {
const char* kSearchIconResourceName = "//resources/images/icon_search.svg";
}  // namespace searchbox_internal

namespace {

// TODO(niharm): convert back to constexpr char[] once feature is cleaned up
const char* kAnswerCurrencyIconResourceName =
    "//resources/cr_components/searchbox/icons/currency.svg";
constexpr char kAnswerDefaultIconResourceName[] =
    "//resources/cr_components/searchbox/icons/default.svg";
const char* kAnswerDictionaryIconResourceName =
    "//resources/cr_components/searchbox/icons/definition.svg";
const char* kAnswerFinanceIconResourceName =
    "//resources/cr_components/searchbox/icons/finance.svg";
const char* kAnswerSunriseIconResourceName =
    "//resources/cr_components/searchbox/icons/sunrise.svg";
const char* kAnswerTranslationIconResourceName =
    "//resources/cr_components/searchbox/icons/translation.svg";
const char* kAnswerWhenIsIconResourceName =
    "//resources/cr_components/searchbox/icons/when_is.svg";
const char* kBookmarkIconResourceName = "//resources/images/icon_bookmark.svg";
const char* kCalculatorIconResourceName =
    "//resources/cr_components/searchbox/icons/calculator.svg";
const char* kChromeProductIconResourceName =
    "//resources/cr_components/searchbox/icons/chrome_product.svg";
const char* kClockIconResourceName = "//resources/images/icon_clock.svg";
const char* kDinoIconResourceName =
    "//resources/cr_components/searchbox/icons/dino.svg";
constexpr char kDriveDocsIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_docs.svg";
constexpr char kDriveFolderIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_folder.svg";
constexpr char kDriveFormIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_form.svg";
constexpr char kDriveImageIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_image.svg";
constexpr char kDriveLogoIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_logo.svg";
constexpr char kDrivePdfIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_pdf.svg";
constexpr char kDriveSheetsIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_sheets.svg";
constexpr char kDriveSlidesIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_slides.svg";
constexpr char kDriveVideoIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_video.svg";
constexpr char kEnterpriseIconResourceName[] =
    "//resources/cr_components/searchbox/icons/enterprise.svg";
constexpr char kExtensionAppIconResourceName[] =
    "//resources/cr_components/searchbox/icons/extension_app.svg";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kGoogleCalendarIconResourceName[] =
    "//resources/cr_components/searchbox/icons/calendar.svg";
constexpr char kGoogleAgentspaceIconResourceName[] =
    "//resources/cr_components/searchbox/icons/google_agentspace_logo.svg";
constexpr char kGoogleAgentspace25IconResourceName[] =
    "//resources/cr_components/searchbox/icons/google_agentspace_logo_25.svg";
const char* kGoogleGIconResourceName =
    "//resources/cr_components/searchbox/icons/google_g.svg";
constexpr char kGoogleKeepNoteIconResourceName[] =
    "//resources/cr_components/searchbox/icons/note.svg";
constexpr char kGoogleSitesIconResourceName[] =
    "//resources/cr_components/searchbox/icons/sites.svg";
constexpr char kGoogleLensMonochromeLogoIcon[] =
    "//resources/cr_components/searchbox/icons/camera.svg";
constexpr char kGoogleAgentspaceMonochromeLogoIcon[] =
    "//resources/cr_components/searchbox/icons/"
    "google_agentspace_monochrome_logo.svg";
constexpr char kGoogleAgentspaceMonochromeLogo25Icon[] =
    "//resources/cr_components/searchbox/icons/"
    "google_agentspace_monochrome_logo_25.svg";
#endif
const char* kHistoryIconResourceName = "//resources/images/icon_history.svg";
const char* kIncognitoIconResourceName =
    "//resources/cr_components/searchbox/icons/incognito.svg";
const char* kJourneysIconResourceName =
    "//resources/cr_components/searchbox/icons/journeys.svg";
const char* kPageIconResourceName =
    "//resources/cr_components/searchbox/icons/page.svg";
const char* kPedalsIconResourceName = "//theme/current-channel-logo";
const char* kSearchSparkIconResourceName =
    "//resources/cr_components/searchbox/icons/search_spark.svg";
const char* kSparkIconResourceName =
    "//resources/cr_components/searchbox/icons/spark.svg";
const char* kStarActiveIconResourceName =
    "//resources/cr_components/searchbox/icons/star_active.svg";
const char* kSubdirectoryArrowRightResourceName =
    "//resources/cr_components/searchbox/icons/subdirectory_arrow_right.svg";
const char* kTabIconResourceName =
    "//resources/cr_components/searchbox/icons/tab.svg";
const char* kTrendingUpIconResourceName =
    "//resources/cr_components/searchbox/icons/trending_up.svg";

#if BUILDFLAG(IS_MAC)
const char* kMacShareIconResourceName =
    "//resources/cr_components/searchbox/icons/mac_share.svg";
#elif BUILDFLAG(IS_WIN)
const char* kWinShareIconResourceName =
    "//resources/cr_components/searchbox/icons/win_share.svg";
#elif BUILDFLAG(IS_LINUX)
const char* kLinuxShareIconResourceName =
    "//resources/cr_components/searchbox/icons/share.svg";
#else
const char* kShareIconResourceName =
    "//resources/cr_components/searchbox/icons/share.svg";
#endif

static void DefineChromeRefreshRealboxIcons() {
  kAnswerCurrencyIconResourceName =
      "//resources/cr_components/searchbox/icons/currency_cr23.svg";
  kAnswerDictionaryIconResourceName =
      "//resources/cr_components/searchbox/icons/definition_cr23.svg";
  kAnswerFinanceIconResourceName =
      "//resources/cr_components/searchbox/icons/finance_cr23.svg";
  kAnswerSunriseIconResourceName =
      "//resources/cr_components/searchbox/icons/sunrise_cr23.svg";
  kAnswerTranslationIconResourceName =
      "//resources/cr_components/searchbox/icons/translation_cr23.svg";
  kAnswerWhenIsIconResourceName =
      "//resources/cr_components/searchbox/icons/when_is_cr23.svg";
  kBookmarkIconResourceName =
      "//resources/cr_components/searchbox/icons/bookmark_cr23.svg";
  kCalculatorIconResourceName =
      "//resources/cr_components/searchbox/icons/calculator_cr23.svg";
  kChromeProductIconResourceName =
      "//resources/cr_components/searchbox/icons/chrome_product_cr23.svg";
  kClockIconResourceName =
      "//resources/cr_components/searchbox/icons/clock_cr23.svg";
  kDinoIconResourceName =
      "//resources/cr_components/searchbox/icons/dino_cr23.svg";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  kGoogleGIconResourceName =
      "//resources/cr_components/searchbox/icons/google_g_cr23.svg";
#endif

  kHistoryIconResourceName =
      "//resources/cr_components/searchbox/icons/history_cr23.svg";
  kIncognitoIconResourceName =
      "//resources/cr_components/searchbox/icons/incognito_cr23.svg";
  kJourneysIconResourceName =
      "//resources/cr_components/searchbox/icons/journeys_cr23.svg";
  kPageIconResourceName =
      "//resources/cr_components/searchbox/icons/page_cr23.svg";
  kPedalsIconResourceName =
      "//resources/cr_components/searchbox/icons/chrome_product_cr23.svg";
  searchbox_internal::kSearchIconResourceName =
      "//resources/cr_components/searchbox/icons/search_cr23.svg";
  kTabIconResourceName =
      "//resources/cr_components/searchbox/icons/tab_cr23.svg";
  kTrendingUpIconResourceName =
      "//resources/cr_components/searchbox/icons/trending_up_cr23.svg";

#if BUILDFLAG(IS_MAC)
  kMacShareIconResourceName =
      "//resources/cr_components/searchbox/icons/mac_share_cr23.svg";
#elif BUILDFLAG(IS_WIN)
  kWinShareIconResourceName =
      "//resources/cr_components/searchbox/icons/win_share_cr23.svg";
#elif BUILDFLAG(IS_LINUX)
  kLinuxShareIconResourceName =
      "//resources/cr_components/searchbox/icons/share_cr23.svg";
#else
  kShareIconResourceName =
      "//resources/cr_components/searchbox/icons/share_cr23.svg";
#endif
}

std::u16string GetAdditionalA11yMessage(
    const AutocompleteMatch& match,
    searchbox::mojom::SelectionLineState state) {
  switch (state) {
    case searchbox::mojom::SelectionLineState::kNormal: {
      if (match.has_tab_match.value_or(false)) {
        return l10n_util::GetStringUTF16(IDS_ACC_TAB_SWITCH_SUFFIX);
      }
      const OmniboxAction* action = match.GetActionAt(0u);
      if (action) {
        return action->GetLabelStrings().accessibility_suffix;
      }
      if (match.SupportsDeletion()) {
        return l10n_util::GetStringUTF16(IDS_ACC_REMOVE_SUGGESTION_SUFFIX);
      }
      break;
    }
    case searchbox::mojom::SelectionLineState::kFocusedButtonRemoveSuggestion:
      return l10n_util::GetStringUTF16(
          IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX);
    default:
      NOTREACHED();
  }
  return std::u16string();
}

bool MatchHasSideTypeAndRenderType(
    const AutocompleteMatch& match,
    omnibox::GroupConfig_SideType side_type,
    omnibox::GroupConfig_RenderType render_type,
    const omnibox::GroupConfigMap& suggestion_groups_map) {
  omnibox::GroupId group_id =
      match.suggestion_group_id.value_or(omnibox::GROUP_INVALID);
  return base::Contains(suggestion_groups_map, group_id) &&
         suggestion_groups_map.at(group_id).side_type() == side_type &&
         suggestion_groups_map.at(group_id).render_type() == render_type;
}

std::string GetBase64UrlVariations(Profile* profile) {
  variations::VariationsClient* provider = profile->GetVariationsClient();

  variations::mojom::VariationsHeadersPtr headers =
      provider->GetVariationsHeaders();
  if (headers.is_null()) {
    return std::string();
  }
  const std::string variations_base64 = headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);

  // Variations headers are base64 encoded, however, we're attaching the value
  // to a URL query parameter so they need to be base64url encoded.
  std::string variations_decoded;
  base::Base64Decode(variations_base64, &variations_decoded);

  std::string variations_base64url;
  base::Base64UrlEncode(variations_decoded,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &variations_base64url);

  return variations_base64url;
}

}  // namespace

// static
void SearchboxHandler::SetupWebUIDataSource(content::WebUIDataSource* source,
                                            Profile* profile,
                                            bool enable_voice_search,
                                            bool enable_lens_search) {
  // The WebUI Omnibox code will override this to `true` to adjust various
  // color and layout options.
  source->AddBoolean("isTopChromeSearchbox", false);
  // The lens searchboxes overrides this to true to adjust various color and
  // layout options.
  source->AddBoolean("isLensSearchbox", false);

  source->AddBoolean("reportMetrics", false);
  source->AddString("charTypedToPaintMetricName", "");
  source->AddString("resultChangedToPaintMetricName", "");

  source->AddBoolean("forceHideEllipsis", false);
  source->AddBoolean("enableThumbnailSizingTweaks", false);
  source->AddBoolean("enableCsbMotionTweaks", false);

  static constexpr webui::LocalizedString kStrings[] = {
      {"lensSearchButtonLabel", IDS_TOOLTIP_LENS_SEARCH},
      {"searchboxSeparator", IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR},
      {"removeSuggestion", IDS_OMNIBOX_REMOVE_SUGGESTION},
      {"searchBoxHint", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD},
      {"searchBoxHintMultimodal", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MULTIMODAL},
      {"searchboxThumbnailLabel",
       IDS_GOOGLE_SEARCH_BOX_MULTIMODAL_IMAGE_THUMBNAIL},
      {"voiceSearchButtonLabel", IDS_TOOLTIP_MIC_SEARCH},

      // Composebox.
      {"addContext", IDS_NTP_COMPOSE_ADD_CONTEXTS},
      {"addContextTitle", IDS_NTP_COMPOSE_ADD_CONTEXT_TITLE},
      {"addImage", IDS_NTP_COMPOSE_ADD_IMAGE},
      {"addTab", IDS_NTP_COMPOSE_MOST_RECENT_TABS},
      {"dismissButton", IDS_NTP_DISMISS},
      // TODO(b/467036804): Update the value of `lensSearchAriaLabel`.
      {"lensSearchAriaLabel", IDS_CONTENT_CONTEXT_LENS_OVERLAY},
      {"lensSearchLabel", IDS_CONTENT_CONTEXT_LENS_OVERLAY},
      {"searchboxComposeButtonText", IDS_NTP_COMPOSE_ENTRYPOINT},
      {"searchboxComposeButtonTitle", IDS_NTP_COMPOSE_ENTRYPOINT_A11Y_LABEL},
      {"composeboxCancelButtonTitle", IDS_NTP_COMPOSE_CANCEL_BUTTON_A11Y_LABEL},
      {"composeboxCancelButtonTitleInput",
       IDS_NTP_COMPOSE_CANCEL_BUTTON_A11Y_LABEL_INPUT},
      {"composeboxImageUploadButtonTitle",
       IDS_NTP_COMPOSE_IMAGE_UPLOAD_BUTTON_A11Y_LABEL},
      {"composeboxPdfUploadButtonTitle",
       IDS_NTP_COMPOSE_PDF_UPLOAD_BUTTON_A11Y_LABEL},
      {"composeboxPlaceholderText", IDS_NTP_COMPOSE_PLACEHOLDER_TEXT},
      {"composeboxSmartComposeTabTitle", IDS_NTP_COMPOSE_SMART_COMPOSE_TAB},
      {"composeboxSmartComposeTitle", IDS_NTP_COMPOSE_SMART_COMPOSE_A11Y_LABEL},
      {"composeboxSubmitButtonTitle", IDS_NTP_COMPOSE_SUBMIT_BUTTON_A11Y_LABEL},
      {"composeboxDeleteFileTitle", IDS_NTP_COMPOSE_DELETE_FILE_A11Y_LABEL},
      {"composeboxFileUploadStartedText",
       IDS_NTP_COMPOSE_FILE_UPLOAD_STARTED_A11Y_TEXT},
      {"composeboxFileUploadCompleteText",
       IDS_NTP_COMPOSE_FILE_UPLOAD_COMPLETE_A11Y_TEXT},
      {"composeboxFileUploadInvalidEmptySize",
       IDS_NTP_COMPOSE_FILE_UPLOAD_INVALID_EMPTY_SIZE},
      {"composeboxFileUploadInvalidTooLarge",
       IDS_NTP_COMPOSE_FILE_UPLOAD_INVALID_TOO_LARGE},
      {"composeboxFileUploadImageProcessingError",
       IDS_NTP_COMPOSE_FILE_UPLOAD_IMAGE_PROCESSING_ERROR},
      {"composeboxFileUploadValidationFailed",
       IDS_NTP_COMPOSE_FILE_UPLOAD_VALIDATION_FAILED},
      {"composeboxFileUploadFailed", IDS_NTP_COMPOSE_FILE_UPLOAD_FAILED},
      {"composeboxFileUploadExpired", IDS_NTP_COMPOSE_FILE_UPLOAD_EXPIRED},
      {"menu", IDS_MENU},
      {"uploadFile", IDS_NTP_COMPOSE_ADD_FILE},
      {"deepSearch", IDS_NTP_COMPOSE_DEEP_SEARCH},
      {"createImages", IDS_NTP_COMPOSE_CREATE_IMAGES},
      {"composeDeepSearchPlaceholder", IDS_COMPOSE_DEEP_SEARCH_PLACEHOLDER},
      {"composeCreateImagePlaceholder", IDS_COMPOSE_CREATE_IMAGE_PLACEHOLDER},
      {"askAboutThisPage", IDS_WEBUI_OMNIBOX_COMPOSE_ASK_ABOUT_THIS_PAGE},
      {"askAboutThisPageAriaLabel",
       IDS_WEBUI_OMNIBOX_COMPOSE_ASK_ABOUT_THIS_PAGE_ARIA_LABEL},
      {"askAboutThisTab", IDS_COMPOSE_ASK_ABOUT_THIS_TAB},
      {"askAboutThisTabAriaLabel", IDS_COMPOSE_ASK_ABOUT_THIS_TAB_ARIA_LABEL},
      {"removeToolChipAriaLabel", IDS_COMPOSE_REMOVE_TOOL_CHIP_A11Y_LABEL},
      {"composeFileTypesAllowedError",
       IDS_NTP_COMPOSE_FILE_TYPE_NOT_ALLOWED_ERROR},
      {"voiceClose", IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP},
      {"voiceDetails", IDS_NEW_TAB_VOICE_DETAILS},
      {"voiceListening", IDS_NEW_TAB_VOICE_LISTENING},
      {"voicePermissionError", IDS_NEW_TAB_VOICE_PERMISSION_ERROR},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddString("searchboxComposePlaceholder",
                    ntp_composebox::FeatureConfig::Get()
                        .config.composebox()
                        .input_placeholder_text());
  source->AddString(
      "suggestionActivityLink",
      l10n_util::GetStringFUTF16(IDS_NTP_COMPOSE_SUGGESTIONS_INFO,
                                 u"https://myactivity.google.com/"
                                 u"activitycontrols?settings=search&utm_source="
                                 u"aim&utm_campaign=aim_str"));

  source->AddBoolean(
      "searchboxMatchSearchboxTheme",
      base::FeatureList::IsEnabled(ntp_features::kRealboxMatchSearchboxTheme));

  DefineChromeRefreshRealboxIcons();
  source->AddString(
      "searchboxDefaultIcon",
      base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          ? kGoogleGIconResourceName
#else
          ? searchbox_internal::kSearchIconResourceName
#endif
          : searchbox_internal::kSearchIconResourceName);

  source->AddBoolean("searchboxVoiceSearch", enable_voice_search);
  source->AddBoolean("searchboxLensSearch", enable_lens_search);
  source->AddString("searchboxLensVariations", GetBase64UrlVariations(profile));
  source->AddBoolean(
      "searchboxCr23Theming",
      base::FeatureList::IsEnabled(ntp_features::kRealboxCr23Theming));
  source->AddBoolean("searchboxCr23SteadyStateShadow",
                     ntp_features::kNtpRealboxCr23SteadyStateShadow.Get());

  auto composebox_config = ntp_composebox::FeatureConfig::Get().config;
  source->AddString("composeboxDragAndDropHint",
                    l10n_util::GetPluralStringFUTF16(
                        IDS_NTP_COMPOSE_DRAG_AND_DROP_HINT,
                        composebox_config.composebox().max_num_files()));
  source->AddString("maxFilesReachedError",
                    l10n_util::GetPluralStringFUTF16(
                        IDS_NTP_COMPOSE_MAX_FILES_REACHED_ERROR,
                        composebox_config.composebox().max_num_files()));
  source->AddBoolean(
      "searchboxShowComposeAnimation",
      profile->GetPrefs()->GetInteger(
          prefs::kNtpComposeButtonShownCountPrefName) <
          composebox_config.entry_point().num_page_load_animations());
}

std::string SearchboxHandler::AutocompleteIconToResourceName(
    const gfx::VectorIcon& icon) const {
  if (icon.is_empty()) {
    return "";  // An empty resource name is effectively a blank icon.
  }

  // Keep sorted alphabetically by `if` predicate. E.g.
  // - `omnibox::kA`
  // - `omnibox::kB`
  // - `vector_icons::kA`

  if (icon.name == omnibox::kAnswerCurrencyChromeRefreshIcon.name) {
    return kAnswerCurrencyIconResourceName;
  } else if (icon.name == omnibox::kAnswerDefaultIcon.name) {
    return kAnswerDefaultIconResourceName;
  } else if (icon.name == omnibox::kAnswerDictionaryChromeRefreshIcon.name) {
    return kAnswerDictionaryIconResourceName;
  } else if (icon.name == omnibox::kAnswerFinanceChromeRefreshIcon.name) {
    return kAnswerFinanceIconResourceName;
  } else if (icon.name == omnibox::kAnswerSunriseChromeRefreshIcon.name) {
    return kAnswerSunriseIconResourceName;
  } else if (icon.name == omnibox::kAnswerTranslationChromeRefreshIcon.name) {
    return kAnswerTranslationIconResourceName;
  } else if (icon.name == omnibox::kBookmarkChromeRefreshIcon.name) {
    return kBookmarkIconResourceName;
  } else if (icon.name == omnibox::kCalculatorChromeRefreshIcon.name) {
    return kCalculatorIconResourceName;
  } else if (icon.name == omnibox::kDinoCr2023Icon.name) {
    return kDinoIconResourceName;
  } else if (icon.name == omnibox::kDriveDocsIcon.name) {
    return kDriveDocsIconResourceName;
  } else if (icon.name == omnibox::kDriveFolderIcon.name) {
    return kDriveFolderIconResourceName;
  } else if (icon.name == omnibox::kDriveFormsIcon.name) {
    return kDriveFormIconResourceName;
  } else if (icon.name == omnibox::kDriveImageIcon.name) {
    return kDriveImageIconResourceName;
  } else if (icon.name == omnibox::kDriveLogoIcon.name) {
    return kDriveLogoIconResourceName;
  } else if (icon.name == omnibox::kDrivePdfIcon.name) {
    return kDrivePdfIconResourceName;
  } else if (icon.name == omnibox::kDriveSheetsIcon.name) {
    return kDriveSheetsIconResourceName;
  } else if (icon.name == omnibox::kDriveSlidesIcon.name) {
    return kDriveSlidesIconResourceName;
  } else if (icon.name == omnibox::kDriveVideoIcon.name) {
    return kDriveVideoIconResourceName;
  } else if (icon.name == omnibox::kEnterpriseIcon.name) {
    return kEnterpriseIconResourceName;
  } else if (icon.name == omnibox::kExtensionAppIcon.name) {
    return kExtensionAppIconResourceName;
  } else if (icon.name == omnibox::kIncognitoCr2023Icon.name) {
    return kIncognitoIconResourceName;
  } else if (icon.name == omnibox::kJourneysChromeRefreshIcon.name) {
    return kJourneysIconResourceName;
  } else if (icon.name == omnibox::kJourneysIcon.name) {
    return kJourneysIconResourceName;
  } else if (icon.name == omnibox::kPageChromeRefreshIcon.name) {
    return kPageIconResourceName;
  } else if (icon.name == omnibox::kProductChromeRefreshIcon.name) {
    return kPedalsIconResourceName;
  } else if (icon.name == omnibox::kSearchSparkIcon.name) {
    return kSearchSparkIconResourceName;
  } else if (icon.name == omnibox::kSparkIcon.name) {
    return kSparkIconResourceName;
  } else if (icon.name == omnibox::kStarActiveChromeRefreshIcon.name) {
    return kStarActiveIconResourceName;
  } else if (icon.name == omnibox::kSubdirectoryArrowRightIcon.name) {
    return kSubdirectoryArrowRightResourceName;
  } else if (icon.name == omnibox::kSwitchCr2023Icon.name) {
    return kTabIconResourceName;
  } else if (icon.name == omnibox::kTrendingUpChromeRefreshIcon.name) {
    return kTrendingUpIconResourceName;
  } else if (icon.name == vector_icons::kHistoryChromeRefreshIcon.name) {
    return kHistoryIconResourceName;
  } else if (icon.name == vector_icons::kSearchChromeRefreshIcon.name) {
    return searchbox_internal::kSearchIconResourceName;
  }

  // Don't add new icons here. Add them alphabetically by `if` predicate. E.g.
  // - `omnibox::kA`
  // - `omnibox::kB`
  // - `vector_icons::kA`

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (icon.name == vector_icons::kGoogleAgentspaceMonochromeLogoIcon.name) {
    return kGoogleAgentspaceMonochromeLogoIcon;
  } else if (icon.name ==
             vector_icons::kGoogleAgentspaceMonochromeLogo25Icon.name) {
    return kGoogleAgentspaceMonochromeLogo25Icon;
  } else if (icon.name == vector_icons::kGoogleCalendarIcon.name) {
    return kGoogleCalendarIconResourceName;
  } else if (icon.name == vector_icons::kGoogleGLogoMonochromeIcon.name) {
    return kGoogleGIconResourceName;
  } else if (icon.name == vector_icons::kGoogleKeepNoteIcon.name) {
    return kGoogleKeepNoteIconResourceName;
  } else if (icon.name == vector_icons::kGoogleLensLogoIcon.name) {
    // TODO(crbug.com/446957004): Temporarily use the monochrome logo.
    return kGoogleLensMonochromeLogoIcon;
  } else if (icon.name == vector_icons::kGoogleLensMonochromeLogoIcon.name) {
    return kGoogleLensMonochromeLogoIcon;
  } else if (icon.name == vector_icons::kGoogleSitesIcon.name) {
    return kGoogleSitesIconResourceName;
  }
#endif

#if BUILDFLAG(IS_MAC)
  if (icon.name == omnibox::kShareMacChromeRefreshIcon.name) {
    return kMacShareIconResourceName;
  }
#elif BUILDFLAG(IS_WIN)
  if (icon.name == omnibox::kShareWinChromeRefreshIcon.name) {
    return kWinShareIconResourceName;
  }
#elif BUILDFLAG(IS_LINUX)
  if (icon.name == omnibox::kShareLinuxChromeRefreshIcon.name) {
    return kLinuxShareIconResourceName;
  }
#else
  if (icon.name == omnibox::kShareChromeRefreshIcon.name) {
    return kShareIconResourceName;
  }
#endif

  // Don't add new icons here. Add them alphabetically by `if` predicate. E.g.
  // - `omnibox::kA`
  // - `omnibox::kB`
  // - `vector_icons::kA`

  // TODO(446953331): It's error-prone to keep the above if's up to date. When
  //   omnibox input and popup views are replaced with webUI, matches and
  //   actions can store an icon enum instead of `VectorIcon`.
  NOTREACHED() << "Every autocomplete icon must have an equivalent SVG "
                  "resource for the NTP Realbox. icon.name: '"
               << icon.name << "'";
}

searchbox::mojom::AutocompleteResultPtr
SearchboxHandler::CreateAutocompleteResult(
    const std::u16string& input,
    const AutocompleteResult& result,
    const OmniboxEditModel* edit_model,
    bookmarks::BookmarkModel* bookmark_model,
    const PrefService* prefs,
    const TemplateURLService* turl_service) const {
  return searchbox::mojom::AutocompleteResult::New(
      input,
      CreateSuggestionGroupsMap(result, edit_model, prefs,
                                result.suggestion_groups_map()),
      CreateAutocompleteMatches(result, edit_model, bookmark_model,
                                result.suggestion_groups_map(), turl_service),
      base::UTF8ToUTF16(result.smart_compose_inline_hint()));
}

base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr>
SearchboxHandler::CreateSuggestionGroupsMap(
    const AutocompleteResult& result,
    const OmniboxEditModel* edit_model,
    const PrefService* prefs,
    const omnibox::GroupConfigMap& suggestion_groups_map) const {
  base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr> result_map;
  for (const auto& pair : suggestion_groups_map) {
    std::u16string header =
        edit_model->GetSuggestionGroupHeaderText(pair.first);

    if (!header.empty()) {
      searchbox::mojom::SuggestionGroupPtr suggestion_group =
          searchbox::mojom::SuggestionGroup::New();
      suggestion_group->header = header;
      suggestion_group->side_type =
          static_cast<searchbox::mojom::SideType>(pair.second.side_type());
      suggestion_group->render_type =
          static_cast<searchbox::mojom::RenderType>(pair.second.render_type());

      result_map.emplace(static_cast<int>(pair.first),
                         std::move(suggestion_group));
    }
  }
  return result_map;
}

std::vector<searchbox::mojom::AutocompleteMatchPtr>
SearchboxHandler::CreateAutocompleteMatches(
    const AutocompleteResult& result,
    const OmniboxEditModel* edit_model,
    bookmarks::BookmarkModel* bookmark_model,
    const omnibox::GroupConfigMap& suggestion_groups_map,
    const TemplateURLService* turl_service) const {
  std::vector<searchbox::mojom::AutocompleteMatchPtr> matches;
  for (const auto& match : result) {
    auto mojom_match = CreateAutocompleteMatch(
        match, matches.size(), edit_model, bookmark_model,
        suggestion_groups_map, turl_service);
    if (mojom_match) {
      matches.push_back(std::move(mojom_match.value()));
    }
  }
  return matches;
}

std::optional<searchbox::mojom::AutocompleteMatchPtr>
SearchboxHandler::CreateAutocompleteMatch(
    const AutocompleteMatch& match,
    size_t line,
    const OmniboxEditModel* edit_model,
    bookmarks::BookmarkModel* bookmark_model,
    const omnibox::GroupConfigMap& suggestion_groups_map,
    const TemplateURLService* turl_service) const {
  // Skip the primary column horizontal matches. This check guards against
  // this unexpected scenario as the UI expects the primary column matches to
  // be vertical ones.
  if (MatchHasSideTypeAndRenderType(
          match, omnibox::GroupConfig_SideType_DEFAULT_PRIMARY,
          omnibox::GroupConfig_RenderType_HORIZONTAL, suggestion_groups_map)) {
    return std::nullopt;
  }

  // Skip the secondary column horizontal matches that are not entities or do
  // not have images. This check guards against this unexpected scenario as
  // the UI expects the secondary column horizontal matches to be entity
  // suggestions with images.
  if (MatchHasSideTypeAndRenderType(
          match, omnibox::GroupConfig_SideType_SECONDARY,
          omnibox::GroupConfig_RenderType_HORIZONTAL, suggestion_groups_map) &&
      (match.type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
       !match.image_url.is_valid())) {
    return std::nullopt;
  }

  searchbox::mojom::AutocompleteMatchPtr mojom_match =
      searchbox::mojom::AutocompleteMatch::New();
  mojom_match->is_hidden = match.ShouldHideBasedOnStarterPack(turl_service);
  mojom_match->allowed_to_be_default_match = match.allowed_to_be_default_match;
  mojom_match->contents = match.contents;
  for (const auto& contents_class : match.contents_class) {
    mojom_match->contents_class.push_back(
        searchbox::mojom::ACMatchClassification::New(contents_class.offset,
                                                     contents_class.style));
  }
  mojom_match->description = match.description;
  for (const auto& description_class : match.description_class) {
    mojom_match->description_class.push_back(
        searchbox::mojom::ACMatchClassification::New(description_class.offset,
                                                     description_class.style));
  }
  mojom_match->destination_url = match.destination_url;
  mojom_match->suggestion_group_id =
      match.suggestion_group_id.value_or(omnibox::GROUP_INVALID);
  const bool is_bookmarked =
      bookmark_model->IsBookmarked(match.destination_url);
  // For starter pack suggestions, use template url to generate proper vector
  // icon.
  const TemplateURL* associated_keyword_turl =
      match.associated_keyword.empty()
          ? nullptr
          : turl_service->GetTemplateURLForKeyword(match.associated_keyword);
  mojom_match->icon_path = AutocompleteIconToResourceName(
      match.GetVectorIcon(is_bookmarked, associated_keyword_turl));
  // For enterprise search aggregator people suggestions, use branded icon if
  // branded build.
  if (match.enterprise_search_aggregator_type ==
      AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE) {
    mojom_match->is_enterprise_search_aggregator_people_type = true;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    mojom_match->icon_path =
        base::FeatureList::IsEnabled(omnibox::kUseAgentspace25Logo)
            ? kGoogleAgentspace25IconResourceName
            : kGoogleAgentspaceIconResourceName;
#endif
  }
  mojom_match->icon_url = match.icon_url;
  // For featured enterprise search suggestions, use template url to generate
  // the proper icon url.
  const TemplateURL* keyword_turl =
      match.keyword.empty()
          ? nullptr
          : turl_service->GetTemplateURLForKeyword(match.keyword);
  if (AutocompleteMatch::IsFeaturedEnterpriseSearchType(match.type) &&
      keyword_turl) {
    GURL favicon_url = keyword_turl->favicon_url();
    if (favicon_url.is_valid()) {
      mojom_match->icon_url = favicon_url;
    }
  }
  mojom_match->image_dominant_color = match.image_dominant_color;
  mojom_match->image_url = match.image_url.spec();
  mojom_match->fill_into_edit = match.fill_into_edit;
  mojom_match->inline_autocompletion = match.inline_autocompletion;
  mojom_match->is_search_type = AutocompleteMatch::IsSearchType(match.type);
  mojom_match->swap_contents_and_description =
      match.swap_contents_and_description;
  mojom_match->type = AutocompleteMatchType::ToString(match.type);
  mojom_match->supports_deletion = match.SupportsDeletion();
  if (match.answer_template.has_value()) {
    const omnibox::AnswerData& answer_data = match.answer_template->answers(0);
    const omnibox::FormattedString& headline = answer_data.headline();
    std::u16string headline_substr;
    if (headline.fragments_size() > 0) {
      const std::string& headline_text = headline.text();
      // Grab the substring of headline starting after the first fragment text
      // ends. Not making use of the first fragment because it contains the
      // same data as `match.contents` but with HTML tags.
      headline_substr = base::UTF8ToUTF16(headline_text.substr(
          headline.fragments(0).text().size(),
          headline_text.size() - headline.fragments(0).text().size()));
    }

    const auto& subhead_text = base::UTF8ToUTF16(answer_data.subhead().text());
    // Reusing SuggestionAnswer because `headline` and `subhead` are
    // equivalent to `first_line` and `second_line`.
    mojom_match->answer = searchbox::mojom::SuggestionAnswer::New(
        headline_substr.empty()
            ? match.contents
            : base::JoinString({match.contents, headline_substr}, u" "),
        subhead_text);
    mojom_match->image_url = answer_data.image().url();
    mojom_match->is_weather_answer_suggestion =
        match.answer_type == omnibox::ANSWER_TYPE_WEATHER;
  }
  mojom_match->is_rich_suggestion =
      !mojom_match->image_url.empty() ||
      match.type == AutocompleteMatchType::CALCULATOR ||
      match.answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED ||
      match.enterprise_search_aggregator_type ==
          AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE;
  for (const auto& action : match.actions) {
    std::string icon_path;
    if (action->GetIconImage().IsEmpty()) {
      icon_path = AutocompleteIconToResourceName(action->GetVectorIcon());
    } else {
      icon_path = webui::GetBitmapDataUrl(action->GetIconImage().AsBitmap());
    }
    const OmniboxAction::LabelStrings& label_strings =
        action->GetLabelStrings();
    mojom_match->actions.emplace_back(searchbox::mojom::Action::New(
        base::UTF16ToUTF8(label_strings.hint),
        base::UTF16ToUTF8(label_strings.suggestion_contents), icon_path,
        base::UTF16ToUTF8(label_strings.accessibility_hint)));
  }
  std::u16string header_text =
      edit_model->GetSuggestionGroupHeaderText(match.suggestion_group_id);
  mojom_match->a11y_label = AutocompleteMatchType::ToAccessibilityLabel(
      match, header_text, match.contents, line, 0,
      GetAdditionalA11yMessage(match,
                               searchbox::mojom::SelectionLineState::kNormal));

  mojom_match->remove_button_a11y_label =
      AutocompleteMatchType::ToAccessibilityLabel(
          match, header_text, match.contents, line, 0,
          GetAdditionalA11yMessage(match, searchbox::mojom::SelectionLineState::
                                              kFocusedButtonRemoveSuggestion));

  mojom_match->tail_suggest_common_prefix = match.tail_suggest_common_prefix;

  mojom_match->is_noncanned_aim_suggestion =
      match.suggestion_group_id == omnibox::GROUP_MIA_RECOMMENDATIONS;

  return mojom_match;
}

SearchboxHandler::SearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<OmniboxController> controller)
    : profile_(profile),
      web_contents_(web_contents),
      owned_controller_(std::move(controller)),
      page_handler_(this, std::move(pending_page_handler)) {
  controller_ = owned_controller_.get();
}

SearchboxHandler::~SearchboxHandler() {
  // Avoids dangling pointer warning when `controller_` is not owned.
  controller_ = nullptr;
}

bool SearchboxHandler::IsRemoteBound() const {
  return page_.is_bound();
}

void SearchboxHandler::AddFileContextFromBrowser(
    base::UnguessableToken token,
    searchbox::mojom::SelectedFileInfoPtr file_info) {
  if (page_ && IsRemoteBound()) {
    page_->AddFileContext(token, std::move(file_info));
  }
}

void SearchboxHandler::OnContextualInputStatusChanged(
    base::UnguessableToken token,
    composebox_query::mojom::FileUploadStatus status,
    std::optional<composebox_query::mojom::FileUploadErrorType> error_type) {
  if (page_ && IsRemoteBound()) {
    page_->OnContextualInputStatusChanged(token, status, error_type);
  }
}

void SearchboxHandler::SetPage(
    mojo::PendingRemote<searchbox::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
  if (page_is_bound_callback_for_testing_) {
    std::move(page_is_bound_callback_for_testing_).Run();
  }
}

void SearchboxHandler::OnFocusChanged(bool focused) {
  if (focused) {
    edit_model()->OnSetFocus(false);
  } else {
    edit_model()->OnWillKillFocus();
    edit_model()->OnKillFocus();
  }
}

void SearchboxHandler::QueryAutocomplete(const std::u16string& input,
                                         bool prevent_inline_autocomplete) {
  // TODO(tommycli): We use the input being empty as a signal we are requesting
  // on-focus suggestions. It would be nice if we had a more explicit signal.
  bool is_on_focus = input.empty();

  // Early exit if a query is already in progress for on focus inputs.
  if (!autocomplete_controller()->done() && is_on_focus) {
    return;
  }

  // This will SetInputInProgress and consequently mark the input timer so that
  // Omnibox.TypingDuration will be logged correctly.
  edit_model()->SetUserText(input);

  // RealboxOmniboxClient::GetPageClassification() ignores the arguments.
  const auto page_classification =
      omnibox_controller()->client()->GetPageClassification(
          /*is_prefetch=*/false);
  AutocompleteInput autocomplete_input(
      input, page_classification, ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_current_url(controller_->client()->GetURL());
  autocomplete_input.set_focus_type(
      is_on_focus ? metrics::OmniboxFocusType::INTERACTION_FOCUS
                  : metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);
  // Disable keyword matches as NTP realbox has no UI affordance for it.
  autocomplete_input.set_prefer_keyword(false);
  autocomplete_input.set_allow_exact_keyword_match(false);
  // Set the lens overlay suggest inputs, if available.
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    // Don't set lens params if in "Create Image" mode. This prevents the
    // contextual client from being used in this tool mode.
    if (GetAimToolMode() !=
        omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD) {
      autocomplete_input.set_lens_overlay_suggest_inputs(*suggest_inputs);
    }
  }
  if (controller_->client()->GetContextualInputData().has_value()) {
    auto context_data = controller_->client()->GetContextualInputData().value();
    if (context_data.page_title.has_value() &&
        context_data.page_url.has_value()) {
      autocomplete_input.set_context_tab_title(
          base::UTF8ToUTF16(context_data.page_title.value()));
      autocomplete_input.set_context_tab_url(context_data.page_url.value());
    }
  }

  autocomplete_input.set_aim_tool_mode(GetAimToolMode());

  edit_model()->SetAutocompleteInput(autocomplete_input);
  omnibox_controller()->StartAutocomplete(autocomplete_input);
}

void SearchboxHandler::StopAutocomplete(bool clear_result) {
  omnibox_controller()->StopAutocomplete(clear_result);
}

void SearchboxHandler::OpenAutocompleteMatch(uint8_t line,
                                             const GURL& url,
                                             bool are_matches_showing,
                                             uint8_t mouse_button,
                                             bool alt_key,
                                             bool ctrl_key,
                                             bool meta_key,
                                             bool shift_key) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  edit_model()->OpenSelection(OmniboxPopupSelection(line), timestamp,
                              disposition);
}

void SearchboxHandler::OnNavigationLikely(
    uint8_t line,
    const GURL& url,
    omnibox::mojom::NavigationPredictor navigation_predictor) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }

  if (auto* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    search_prefetch_service->OnNavigationLikely(
        line, *match, navigation_predictor, web_contents_);
  }

  if (SearchPreloadService* search_preload_service =
          SearchPreloadServiceFactory::GetForProfile(profile_)) {
    search_preload_service->OnNavigationLikely(
        line, *match, navigation_predictor, web_contents_);
  }
}

void SearchboxHandler::DeleteAutocompleteMatch(uint8_t line, const GURL& url) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match || !match->SupportsDeletion()) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  omnibox_controller()->StopAutocomplete(/*clear_result=*/false);
  autocomplete_controller()->DeleteMatch(*match);
}

void SearchboxHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {
  // Generic searchbox should not show keywords.
  NOTREACHED();
}

void SearchboxHandler::ShowContextMenu(const gfx::Point& point) {
  // Generic searchbox should not have a context menu.
  NOTREACHED();
}

void SearchboxHandler::ExecuteAction(uint8_t line,
                                     uint8_t action_index,
                                     const GURL& url,
                                     base::TimeTicks match_selection_timestamp,
                                     uint8_t mouse_button,
                                     bool alt_key,
                                     bool ctrl_key,
                                     bool meta_key,
                                     bool shift_key) {
  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  if (action_index >= match->actions.size()) {
    return;
  }
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  OmniboxPopupSelection selection(
      line, OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION,
      action_index);
  edit_model()->OpenSelection(selection, match_selection_timestamp,
                              disposition);
}

void SearchboxHandler::GetPlaceholderConfig(
    GetPlaceholderConfigCallback callback) {
  const auto placeholder_config = ntp_composebox::FeatureConfig::Get()
                                      .config.composebox()
                                      .placeholder_config();
  std::vector<std::u16string> placeholders = {};
  for (auto& text : placeholder_config.placeholders()) {
    switch (text) {
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_ASK:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_ASK_GOOGLE));
        break;
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_PLAN:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_PLAN));
        break;
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_COMPARE:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_COMPARE));
        break;
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_RESEARCH:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_RESEARCH));
        break;
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_TEACH:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_TEACH));
        break;
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_WRITE:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_WRITE));
        break;
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_IMAGE:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_IMAGE));
        break;
      case omnibox::NTPComposeboxConfig_PlaceholderConfig_Placeholder_ASK_TAB:
        placeholders.emplace_back(l10n_util::GetStringUTF16(
            IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_TAB));
        break;
      default:
        NOTREACHED();
    }
  }

  searchbox::mojom::PlaceholderConfigPtr config =
      searchbox::mojom::PlaceholderConfig::New();
  config->texts = std::move(placeholders);
  config->change_text_animation_interval = base::Milliseconds(
      placeholder_config.change_text_animation_interval_ms());
  config->fade_text_animation_duration =
      base::Milliseconds(placeholder_config.fade_text_animation_duration_ms());
  std::move(callback).Run(std::move(config));
}

void SearchboxHandler::GetRecentTabs(GetRecentTabsCallback callback) {
  std::move(callback).Run({});
}

void SearchboxHandler::OnResultChanged(AutocompleteController* controller,
                                       bool default_match_changed) {
  page_->AutocompleteResultChanged(CreateAutocompleteResult(
      autocomplete_controller()->input().text(),
      autocomplete_controller()->result(), edit_model(),
      BookmarkModelFactory::GetForBrowserContext(profile_),
      profile_->GetPrefs(),
      omnibox_controller()->client()->GetTemplateURLService()));

  // The owned `OmniboxController` does not observe the
  // `AutocompleteController`. Notify the prerender here to start preloading if
  // the results are ready.
  // TODO(crbug.com/40062053): Make the owned `OmniboxController` observe the
  //  `AutocompleteController` and move this logic to the
  //  `RealboxOmniboxClient`.
  if (owned_controller_) {
    if (autocomplete_controller()->done()) {
      if (auto* dictionary_preload_service =
              AutocompleteDictionaryPreloadServiceFactory::GetForProfile(
                  profile_)) {
        dictionary_preload_service->MaybePreload(
            autocomplete_controller()->result());
      }
      if (SearchPrefetchService* search_prefetch_service =
              SearchPrefetchServiceFactory::GetForProfile(profile_)) {
        search_prefetch_service->OnResultChanged(
            web_contents_, autocomplete_controller()->result());
      }

      if (SearchPreloadService* search_preload_service =
              SearchPreloadServiceFactory::GetForProfile(profile_)) {
        search_preload_service->OnAutocompleteResultChanged(
            web_contents_, autocomplete_controller()->result());
      }
    }
  }
}

const AutocompleteMatch* SearchboxHandler::GetMatchWithUrl(
    size_t index,
    const GURL& url) const {
  const AutocompleteResult& result = autocomplete_controller()->result();
  if (index >= result.size()) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return nullptr;
  }
  const AutocompleteMatch& match = result.match_at(index);
  if (match.destination_url != url) {
    // This can happen also, for the same reason. We could search the result
    // for the match with this URL, but there would be no guarantee that it's
    // the same match, so for this edge case we treat result mismatch as none.
    return nullptr;
  }
  return &match;
}

omnibox::ChromeAimToolsAndModels SearchboxHandler::GetAimToolMode() const {
  return omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED;
}

OmniboxController* SearchboxHandler::omnibox_controller() const {
  return controller_;
}

AutocompleteController* SearchboxHandler::autocomplete_controller() const {
  return omnibox_controller()->autocomplete_controller();
}

void SearchboxHandler::set_page_is_bound_callback_for_testing(
    base::OnceClosure callback) {
  if (page_.is_bound() && callback) {
    std::move(callback).Run();
    return;
  }
  page_is_bound_callback_for_testing_ = std::move(callback);
}

OmniboxEditModel* SearchboxHandler::edit_model() const {
  return omnibox_controller()->edit_model();
}
