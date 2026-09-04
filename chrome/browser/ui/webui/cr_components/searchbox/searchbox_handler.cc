// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
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
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/contextual_search_provider.h"
#include "components/omnibox/browser/fusebox_action.mojom.h"
#include "components/omnibox/browser/fusebox_action_mojo_utils.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_metrics_constants.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/searchbox_utils.h"
#include "components/omnibox/browser/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/window_open_disposition.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"
#endif
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_client.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "third_party/omnibox_proto/rule_set.pb.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/new_tab_page/new_tab_page_util.h"  // nogncheck
#include "chrome/browser/ui/tabs/tab_strip_model.h"         // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)

namespace searchbox_internal {

const char* kSearchSparkIconResourceName =
    "//resources/cr_components/searchbox/icons/search_spark.svg";
const char* kReplyRotated180IconResourceName =
    "//resources/cr_components/searchbox/icons/reply_rotated180.svg";
}  // namespace searchbox_internal

namespace {

std::u16string GetSmartTabSharingMegaplusString() {
  switch (contextual_tasks::kSmartTabSharingMegaplusStringOption.Get()) {
    case contextual_tasks::SmartTabSharingMegaplusStringOption::kMegaplusV1:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS);
    case contextual_tasks::SmartTabSharingMegaplusStringOption::kMegaplusV2:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS_V2);
    case contextual_tasks::SmartTabSharingMegaplusStringOption::kMegaplusV3:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS_V3);
    default:
      return l10n_util::GetStringUTF16(
          IDS_STS_MEGAPLUS_SHARE_RELEVANT_OPEN_TABS);
  }
}

constexpr int kPromptHeightBuffer = 40;
constexpr int kPromptWidthBuffer = 40;

constexpr char kAnswerCurrencyIconResourceName[] =
    "//resources/cr_components/searchbox/icons/currency_cr23.svg";
constexpr char kAnswerDefaultIconResourceName[] =
    "//resources/cr_components/searchbox/icons/default.svg";
constexpr char kAnswerDictionaryIconResourceName[] =
    "//resources/cr_components/searchbox/icons/definition_cr23.svg";
constexpr char kAnswerFinanceIconResourceName[] =
    "//resources/cr_components/searchbox/icons/finance_cr23.svg";
constexpr char kAnswerSunriseIconResourceName[] =
    "//resources/cr_components/searchbox/icons/sunrise_cr23.svg";
constexpr char kAnswerTranslationIconResourceName[] =
    "//resources/cr_components/searchbox/icons/translation_cr23.svg";
constexpr char kBookmarkIconResourceName[] =
    "//resources/cr_components/searchbox/icons/bookmark_cr23.svg";
constexpr char kCalculatorIconResourceName[] =
    "//resources/cr_components/searchbox/icons/calculator_cr23.svg";
constexpr char kDinoIconResourceName[] =
    "//resources/cr_components/searchbox/icons/dino_cr23.svg";
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
constexpr char kGoogleGIconResourceName[] =
    "//resources/cr_components/searchbox/icons/google_g_cr23.svg";
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
constexpr char kHistoryIconResourceName[] =
    "//resources/cr_components/searchbox/icons/history_cr23.svg";
constexpr char kIncognitoIconResourceName[] =
    "//resources/cr_components/searchbox/icons/incognito_cr23.svg";
constexpr char kJourneysIconResourceName[] =
    "//resources/cr_components/searchbox/icons/journeys_cr23.svg";
constexpr char kNotesSparkIconResourceName[] =
    "//resources/cr_components/searchbox/icons/notes_spark.svg";
constexpr char kPageIconResourceName[] =
    "//resources/cr_components/searchbox/icons/page_cr23.svg";
constexpr char kPedalsIconResourceName[] =
    "//resources/cr_components/searchbox/icons/chrome_product_cr23.svg";
constexpr char kSearchIconResourceName[] =
    "//resources/cr_components/searchbox/icons/search_cr23.svg";
constexpr char kSearchOldIconResourceName[] =
    "//resources/cr_components/searchbox/icons/search_cr23_old.svg";
constexpr char kSparkIconResourceName[] =
    "//resources/cr_components/searchbox/icons/spark.svg";
constexpr char kStarActiveIconResourceName[] =
    "//resources/cr_components/searchbox/icons/star_active.svg";
constexpr char kSubdirectoryArrowRightResourceName[] =
    "//resources/cr_components/searchbox/icons/subdirectory_arrow_right.svg";
constexpr char kTabIconResourceName[] =
    "//resources/cr_components/searchbox/icons/tab_cr23.svg";
constexpr char kTrendingUpIconResourceName[] =
    "//resources/cr_components/searchbox/icons/trending_up_cr23.svg";

#if BUILDFLAG(IS_MAC)
constexpr char kMacShareIconResourceName[] =
    "//resources/cr_components/searchbox/icons/mac_share_cr23.svg";
#elif BUILDFLAG(IS_WIN)
constexpr char kWinShareIconResourceName[] =
    "//resources/cr_components/searchbox/icons/win_share_cr23.svg";
#elif BUILDFLAG(IS_LINUX)
constexpr char kLinuxShareIconResourceName[] =
    "//resources/cr_components/searchbox/icons/share_cr23.svg";
#else
constexpr char kShareIconResourceName[] =
    "//resources/cr_components/searchbox/icons/share_cr23.svg";
#endif

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
  return suggestion_groups_map.contains(group_id) &&
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

BASE_FEATURE(kDropMismatchedSelections, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

// static
base::DictValue SearchboxHandler::GetWebUIDataSourceDict(Profile* profile) {
  return GetWebUIDataSourceDict(profile, WebUIDataSourceOptions{});
}

// Static:
// Returns if all voice search coherence composeboxes are enabled (the default),
// and there is no override (cobrowsing only composebox is enabled) for voice
// coherence.
bool SearchboxHandler::GetAllVoiceSearchCoherenceComposeboxesEnabled() {
  return base::FeatureList::IsEnabled(
             omnibox::kVoiceSearchCoherenceComposeboxes) &&
         !omnibox::kVoiceSearchCoherenceComposeboxCobrowsingOnly.Get();
}

// Static:
// Returns if cobrowsing voice coherence is enabled, regardless of the other
// surfaces.
bool SearchboxHandler::GetVoiceSearchCoherenceCobrowsingComposeboxEnabled() {
  return base::FeatureList::IsEnabled(
             omnibox::kVoiceSearchCoherenceComposeboxes) ||
         omnibox::kVoiceSearchCoherenceComposeboxCobrowsingOnly.Get();
}

// Static:
// Returns if the new voice search animation/metrics/stop button are enabled,
// regardless of transcription.
bool SearchboxHandler::GetVoiceSearchCoherenceAnySearchboxExperimentEnabled() {
  return base::FeatureList::IsEnabled(
             omnibox::kVoiceSearchCoherenceSearchbox) ||
         omnibox::kVoiceSearchCoherenceSearchboxWithLiveTranscription.Get();
}

// static
base::DictValue SearchboxHandler::GetWebUIDataSourceDict(
    Profile* profile,
    WebUIDataSourceOptions options) {
  base::DictValue dict;

  // The WebUI Omnibox code will override this to `true` to adjust various
  // color and layout options.
  dict.Set("isTopChromeSearchbox", false);
  // The lens searchboxes overrides this to true to adjust various color and
  // layout options.
  dict.Set("isLensSearchbox", false);

  dict.Set("reportMetrics", false);
  dict.Set("charTypedToPaintMetricName", "");
  dict.Set("resultChangedToPaintMetricName", "");

  dict.Set("forceHideEllipsis", false);
  dict.Set("enableThumbnailSizingTweaks", false);
  dict.Set("enableCsbMotionTweaks", false);
  dict.Set("keywordSpaceTriggeringEnabled",
           profile && profile->GetPrefs()
               ? profile->GetPrefs()->GetBoolean(
                     omnibox::kKeywordSpaceTriggeringEnabled)
               : true);

  // Returns if ALL composeboxe surfaces' voice coherence is not gated. Includes
  // new metrics, new animation, new submit/stop buttons, no live transcription.
  // Will be false if "voice search coherence only for cobrowsing" is enabled.
  dict.Set("voiceSearchCoherenceComposeboxesEnabled",
           GetAllVoiceSearchCoherenceComposeboxesEnabled());

  // Returns if cobrowsing composebox voice coherence is not gated.
  // Coherence includes new metrics, new animation, new submit/stop buttons,
  // no live transcription. Other surfaces can also be not gated if this is
  // true.
  dict.Set("voiceSearchCoherenceCobrowsingComposeboxEnabled",
           GetVoiceSearchCoherenceCobrowsingComposeboxEnabled());

  // Enables if voice search ntp searchbox live experiment is on. Includes new
  // metrics, new animation, new submit/stop buttons, no live transcription.
  dict.Set(
      "voiceSearchCoherenceSearchboxNoLiveTranscriptionEnabled",
      base::FeatureList::IsEnabled(omnibox::kVoiceSearchCoherenceSearchbox));

  // Enables if voice search ntp searchbox live experiment is on. Includes new
  // metrics, new animation, new submit/stop buttons, live transcription.
  dict.Set("voiceSearchCoherenceSearchboxWithLiveTranscriptionEnabled",
           omnibox::kVoiceSearchCoherenceSearchboxWithLiveTranscription.Get());

  // Enables if either arm of the voice search ntp searchbox live experiment
  // is on.
  dict.Set("voiceSearchCoherenceAnySearchboxExperimentEnabled",
           GetVoiceSearchCoherenceAnySearchboxExperimentEnabled());

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
      {"addDriveFile", IDS_NTP_COMPOSE_ADD_DRIVE},
      {"addTab", IDS_NTP_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE},
      {"shareTabs", IDS_COMPOSE_ADD_TABS},
      {"recentTabsSuffix", IDS_NTP_COMPOSEBOX_RECENT_TAB_SUFFIX},
      {"currentTabSuffix", IDS_COMPOSE_CURRENT_TAB},
      {"sharingTabsWithGoogle", IDS_COMPOSE_SHARING_TABS_WITH_GOOGLE},
      {"dismissButton", IDS_NTP_DISMISS},
      {"searchboxComposeButtonText", IDS_NTP_COMPOSE_ENTRYPOINT},
      {"searchboxComposeButtonTitle", IDS_NTP_COMPOSE_ENTRYPOINT_A11Y_LABEL},
      {"searchboxComposeButtonA11yLabel",
       IDS_NTP_COMPOSE_ENTRYPOINT_A11Y_LABEL},
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
      {"composeboxFileUploadNotAllowed",
       IDS_NTP_COMPOSE_FILE_UPLOAD_NOT_ALLOWED},
      {"menu", IDS_MENU},
      {"uploadFile", IDS_NTP_COMPOSE_ADD_FILE},
      {"deepSearch", IDS_NTP_COMPOSE_DEEP_SEARCH},
      {"createImages", IDS_NTP_COMPOSE_CREATE_IMAGES},
      {"composeDeepSearchPlaceholder", IDS_COMPOSE_DEEP_SEARCH_PLACEHOLDER},
      {"composeCreateImagePlaceholder", IDS_COMPOSE_CREATE_IMAGE_PLACEHOLDER},
      {"askAboutThisPage", IDS_WEBUI_OMNIBOX_COMPOSE_ASK_ABOUT_THIS_PAGE},
      {"askAboutThisPageAriaLabel",
       IDS_WEBUI_OMNIBOX_COMPOSE_ASK_ABOUT_THIS_PAGE_ARIA_LABEL},
      {"askAboutTab", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL},
      {"askAboutTabAriaLabel",
       IDS_WEBUI_OMNIBOX_COMPOSE_ASK_ABOUT_THIS_PAGE_ARIA_LABEL},
      {"removeToolChipAriaLabel", IDS_COMPOSE_REMOVE_TOOL_CHIP_A11Y_LABEL},
      {"composeFileTypesAllowedError",
       IDS_NTP_COMPOSE_FILE_TYPE_NOT_ALLOWED_ERROR},
      {"voiceClose", IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP},
      {"voiceStop", IDS_FUSEBOX_VOICE_SEARCH_STOP_TITLE},
      {"voiceDetails", IDS_NEW_TAB_VOICE_DETAILS},
      {"voiceListening", IDS_NEW_TAB_VOICE_LISTENING},
      {"voiceWaiting", IDS_NEW_TAB_VOICE_WAITING},
      {"voicePermissionError", IDS_NEW_TAB_VOICE_PERMISSION_ERROR},
      {"audioError", IDS_NEW_TAB_VOICE_AUDIO_ERROR},
      {"languageError", IDS_NEW_TAB_VOICE_LANGUAGE_ERROR},
      {"networkError", IDS_NEW_TAB_VOICE_NETWORK_ERROR},
      {"noTranslation", IDS_NEW_TAB_VOICE_NO_TRANSLATION},
      {"noVoice", IDS_NEW_TAB_VOICE_NO_VOICE},
      {"otherError", IDS_NEW_TAB_VOICE_OTHER_ERROR},
      {"tryAgain", IDS_NEW_TAB_VOICE_TRY_AGAIN},
      {"composeboxContextMenuMostRecentTabs",
       IDS_CONTEXTUAL_TASKS_CONTEXT_MENU_MOST_RECENT_TABS},
      {"composeboxContextMenuGeminiModels",
       IDS_CONTEXTUAL_TASKS_CONTEXT_MENU_GEMINI_MODELS},
      {"canvas", IDS_NTP_COMPOSE_CANVAS},
      {"geminiModelAuto", IDS_NTP_COMPOSE_AUTO_MODEL},
      {"geminiModelThinking", IDS_NTP_COMPOSE_THINKING_3_PRO},
      {"composeboxHintTextAskAboutThese",
       IDS_COMPOSE_HINT_TEXT_ASK_ABOUT_THESE},
      {"composeboxHintTextAskAboutThisImage",
       IDS_COMPOSE_HINT_TEXT_ASK_ABOUT_THIS_IMAGE},
      {"composeboxHintTextAskAboutThisTab",
       IDS_COMPOSE_HINT_TEXT_ASK_ABOUT_THIS_TAB},
      {"composeboxHintTextAskAboutThisDoc",
       IDS_COMPOSE_HINT_TEXT_ASK_ABOUT_THIS_DOC},
  };
  for (const auto& entry : kStrings) {
    dict.Set(entry.name, l10n_util::GetStringUTF16(entry.id));
  }

  int lens_search_hint_id = IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL;
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxAskGAboutThisPage) &&
      omnibox::kAskGLensSearchHintText.Get()) {
    lens_search_hint_id = IDS_TIPS_NOTIFICATIONS_GOOGLE_LENS_TITLE;
  }
  dict.Set("lensSearchHint", l10n_util::GetStringUTF16(lens_search_hint_id));

  dict.Set("searchboxComposePlaceholder", ntp_composebox::FeatureConfig::Get()
                                              .config.composebox()
                                              .input_placeholder_text());
  dict.Set(
      "suggestionActivityLink",
      l10n_util::GetStringFUTF16(IDS_NTP_COMPOSE_SUGGESTIONS_INFO,
                                 u"https://myactivity.google.com/"
                                 u"activitycontrols?settings=search&utm_source="
                                 u"aim&utm_campaign=aim_str"));
  dict.Set("searchboxDefaultIcon", features::IsWebUIRoundedIconsEnabled()
                                       ? kSearchIconResourceName
                                       : kSearchOldIconResourceName);

  dict.Set("searchboxVoiceSearch", options.enable_voice_search);
  dict.Set("searchboxLensSearch", options.enable_lens_search);
  dict.Set("searchboxLensVariations", GetBase64UrlVariations(profile));
  dict.Set("searchboxCr23Theming",
           base::FeatureList::IsEnabled(ntp_features::kRealboxCr23Theming));
  dict.Set("searchboxCr23SteadyStateShadow",
           ntp_features::kNtpRealboxCr23SteadyStateShadow.Get());
  dict.Set(
      "realboxVirtualFocusNavigation",
      base::FeatureList::IsEnabled(features::kRealboxVirtualFocusNavigation));
  dict.Set("omniboxPopupVirtualFocusNavigation",
           base::FeatureList::IsEnabled(
               features::kOmniboxPopupVirtualFocusNavigation));
  dict.Set("lensOverlayVirtualFocusNavigation",
           base::FeatureList::IsEnabled(
               features::kLensOverlayVirtualFocusNavigation));
  dict.Set("omniboxEverywhereVirtualFocusNavigation",
           base::FeatureList::IsEnabled(
               features::kOmniboxEverywhereVirtualFocusNavigation));
  dict.Set("webuiBrowserVirtualFocusNavigation",
           base::FeatureList::IsEnabled(
               features::kWebuiBrowserVirtualFocusNavigation));

  int max_files = omnibox::kDefaultMaxTotalInputs;
  int max_images = max_files;
  int max_pdfs = max_files;
  AimEligibilityService* service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  const omnibox::SearchboxConfig* config =
      service ? service->GetSearchboxConfig() : nullptr;
  if (config && config->has_rule_set()) {
    max_files = config->rule_set().max_total_inputs();
    for (const auto& rule : config->rule_set().input_type_rules()) {
      if (rule.input_type() == omnibox::INPUT_TYPE_LENS_IMAGE) {
        max_images = rule.max_instance();
      } else if (rule.input_type() == omnibox::INPUT_TYPE_LENS_FILE) {
        max_pdfs = rule.max_instance();
      }
    }
  }
  dict.Set("composeboxFileMaxCount", max_files);
  dict.Set("composeboxDragAndDropHint",
           l10n_util::GetPluralStringFUTF16(IDS_NTP_COMPOSE_DRAG_AND_DROP_HINT,
                                            max_files));
  dict.Set("maxFilesReachedError",
           l10n_util::GetPluralStringFUTF16(
               IDS_NTP_COMPOSE_MAX_FILES_REACHED_ERROR, max_files));
  dict.Set("maxImagesReachedError",
           l10n_util::GetPluralStringFUTF16(
               IDS_NTP_COMPOSE_MAX_IMAGES_REACHED_ERROR, max_images));
  dict.Set("maxPdfsReachedError",
           l10n_util::GetPluralStringFUTF16(
               IDS_NTP_COMPOSE_MAX_PDFS_REACHED_ERROR, max_pdfs));

  dict.Set("composeboxContextDragAndDropEnabled",
           options.session_allows_drag_and_drop);

#if !BUILDFLAG(IS_ANDROID)
  auto composebox_config = ntp_composebox::FeatureConfig::Get().config;
  dict.Set("searchboxShowComposeAnimation",
           profile->GetPrefs()->GetInteger(
               prefs::kNtpComposeButtonShownCountPrefName) <
               composebox_config.entry_point().num_page_load_animations());
#else
  // TODO(b/509722915): Implement NTP compose button shown count pref on
  // Android.
  dict.Set("searchboxShowComposeAnimation", false);
#endif
  dict.Set("contextualMenuUsePecApi",
           base::FeatureList::IsEnabled(omnibox::kAimUsePecApi));
  dict.Set(
      "useSearchboxConfigIconIds",
      base::FeatureList::IsEnabled(omnibox::kAimUseSearchboxConfigIconIds));
  dict.Set("ShowContextMenuHeaders",
           ntp_composebox::kShowContextMenuHeaders.Get());
  dict.Set("composeboxSmartTabSharingVisible",
           options.is_lens ? false
                           : contextual_tasks::ContextualTasksContextService::
                                 GetIsSmartTabSharingEnabled(profile));
  dict.Set("stsMegaplusShareRelevantOpenTabs",
           GetSmartTabSharingMegaplusString());

  return dict;
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

  std::string resource_name;
  if (icon.name == (features::IsRoundedIconsEnabled()
                        ? omnibox::kAutorenewIcon.name
                        : omnibox::kAnswerCurrencyChromeRefreshOldIcon.name)) {
    resource_name = kAnswerCurrencyIconResourceName;
  } else if (icon.name == omnibox::kAnswerDefaultIcon.name) {
    resource_name = kAnswerDefaultIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kBookIcon.name
                  : omnibox::kAnswerDictionaryChromeRefreshOldIcon.name)) {
    resource_name = kAnswerDictionaryIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kSwapVertIcon.name
                  : omnibox::kAnswerFinanceChromeRefreshOldIcon.name)) {
    resource_name = kAnswerFinanceIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kWbSunnyIcon.name
                  : omnibox::kAnswerSunriseChromeRefreshOldIcon.name)) {
    resource_name = kAnswerSunriseIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kTranslateIcon.name
                  : omnibox::kAnswerTranslationChromeRefreshOldIcon.name)) {
    resource_name = kAnswerTranslationIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kStarIcon.name
                               : omnibox::kBookmarkChromeRefreshOldIcon.name)) {
    resource_name = kBookmarkIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kEqualIcon.name
                  : omnibox::kCalculatorChromeRefreshOldIcon.name)) {
    resource_name = kCalculatorIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kOfflineDinoIcon.name
                               : omnibox::kDinoCr2023OldIcon.name)) {
    resource_name = kDinoIconResourceName;
  } else if (icon.name == omnibox::kDriveDocsCustomIcon.name) {
    resource_name = kDriveDocsIconResourceName;
  } else if (icon.name == omnibox::kDriveFolderCustomIcon.name) {
    resource_name = kDriveFolderIconResourceName;
  } else if (icon.name == omnibox::kDriveFormsCustomIcon.name) {
    resource_name = kDriveFormIconResourceName;
  } else if (icon.name == omnibox::kDriveImageCustomIcon.name) {
    resource_name = kDriveImageIconResourceName;
  } else if (icon.name == omnibox::kDriveLogoCustomIcon.name) {
    resource_name = kDriveLogoIconResourceName;
  } else if (icon.name == omnibox::kDrivePdfCustomIcon.name) {
    resource_name = kDrivePdfIconResourceName;
  } else if (icon.name == omnibox::kDriveSheetsCustomIcon.name) {
    resource_name = kDriveSheetsIconResourceName;
  } else if (icon.name == omnibox::kDriveSlidesCustomIcon.name) {
    resource_name = kDriveSlidesIconResourceName;
  } else if (icon.name == omnibox::kDriveVideoCustomIcon.name) {
    resource_name = kDriveVideoIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kDomainIcon.name
                               : omnibox::kEnterpriseOldIcon.name)) {
    resource_name = kEnterpriseIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kExtensionFilledIcon.name
                               : omnibox::kExtensionAppOldIcon.name)) {
    resource_name = kExtensionAppIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kIncognitoIcon.name
                               : omnibox::kIncognitoCr2023OldIcon.name)) {
    resource_name = kIncognitoIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kConversionPathIcon.name
                               : omnibox::kJourneysChromeRefreshOldIcon.name)) {
    resource_name = kJourneysIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kConversionPathIcon.name
                               : omnibox::kJourneysOldIcon.name)) {
    resource_name = kJourneysIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kNotesSparkIcon.name
                               : omnibox::kNotesSparkOldIcon.name)) {
    resource_name = kNotesSparkIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kPublicIcon.name
                               : omnibox::kPageChromeRefreshOldIcon.name)) {
    resource_name = kPageIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kChromeProductIcon.name
                               : omnibox::kProductChromeRefreshOldIcon.name)) {
    resource_name = kPedalsIconResourceName;
  } else if (icon.name == omnibox::kReplyRotated180CustomIcon.name) {
    resource_name = searchbox_internal::kReplyRotated180IconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kSearchSparkIcon.name
                               : omnibox::kSearchSparkOldIcon.name)) {
    resource_name = searchbox_internal::kSearchSparkIconResourceName;
  } else if (icon.name == omnibox::kSparkIcon.name) {
    resource_name = kSparkIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kStarFilledIcon.name
                  : omnibox::kStarActiveChromeRefreshOldIcon.name)) {
    resource_name = kStarActiveIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kSubdirectoryArrowRightIcon.name
                  : omnibox::kSubdirectoryArrowRightOldIcon.name)) {
    resource_name = kSubdirectoryArrowRightResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? omnibox::kTabIcon.name
                               : omnibox::kSwitchCr2023OldIcon.name)) {
    resource_name = kTabIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? omnibox::kTrendingUpIcon.name
                  : omnibox::kTrendingUpChromeRefreshOldIcon.name)) {
    resource_name = kTrendingUpIconResourceName;
  } else if (icon.name == (features::IsRoundedIconsEnabled()
                               ? vector_icons::kDevicesIcon.name
                               : vector_icons::kDevicesOldIcon.name)) {
    resource_name = kTabIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? vector_icons::kHistoryIcon.name
                  : vector_icons::kHistoryChromeRefreshOldIcon.name)) {
    resource_name = kHistoryIconResourceName;
  } else if (icon.name ==
             (features::IsRoundedIconsEnabled()
                  ? vector_icons::kSearchIcon.name
                  : vector_icons::kSearchChromeRefreshOldIcon.name)) {
    resource_name = kSearchIconResourceName;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (icon.name == vector_icons::kGoogleAgentspaceMonochromeLogoIcon.name) {
    resource_name = kGoogleAgentspaceMonochromeLogoIcon;
  } else if (icon.name ==
             vector_icons::kGoogleAgentspaceMonochromeLogo25Icon.name) {
    resource_name = kGoogleAgentspaceMonochromeLogo25Icon;
  } else if (icon.name == vector_icons::kGoogleCalendarIcon.name) {
    resource_name = kGoogleCalendarIconResourceName;
  } else if (icon.name == vector_icons::kGoogleGLogoMonochromeIcon.name) {
    resource_name = kGoogleGIconResourceName;
  } else if (icon.name == vector_icons::kGoogleKeepNoteIcon.name) {
    resource_name = kGoogleKeepNoteIconResourceName;
  } else if (icon.name == vector_icons::kGoogleLensLogoIcon.name ||
             icon.name == vector_icons::kGoogleLensMonochromeLogoIcon.name) {
    // TODO(crbug.com/446957004): Temporarily use the monochrome logo.
    resource_name = kGoogleLensMonochromeLogoIcon;
  } else if (icon.name == vector_icons::kGoogleSitesIcon.name) {
    resource_name = kGoogleSitesIconResourceName;
  }
#endif

#if BUILDFLAG(IS_MAC)
  if (icon.name == (features::IsRoundedIconsEnabled()
                        ? omnibox::kIosShareIcon.name
                        : omnibox::kShareMacChromeRefreshOldIcon.name)) {
    resource_name = kMacShareIconResourceName;
  }
#elif BUILDFLAG(IS_WIN)
  if (icon.name == (features::IsRoundedIconsEnabled()
                        ? omnibox::kShareWindowsIcon.name
                        : omnibox::kShareWinChromeRefreshOldIcon.name)) {
    resource_name = kWinShareIconResourceName;
  }
#elif BUILDFLAG(IS_LINUX)
  if (icon.name == (features::IsRoundedIconsEnabled()
                        ? omnibox::kSendIcon.name
                        : omnibox::kShareLinuxChromeRefreshOldIcon.name)) {
    resource_name = kLinuxShareIconResourceName;
  }
#else
  if (icon.name == (features::IsRoundedIconsEnabled()
                        ? omnibox::kShareIcon.name
                        : omnibox::kShareChromeRefreshOldIcon.name)) {
    resource_name = kShareIconResourceName;
  }
#endif

  if (resource_name.empty()) {
    DUMP_WILL_BE_NOTREACHED()
        << "Every autocomplete icon must have an equivalent SVG "
           "resource for the NTP Realbox. icon.name: '"
        << icon.name << "'";
    return "";
  }

  if (!features::IsWebUIRoundedIconsEnabled()) {
    static const base::NoDestructor<base::flat_set<std::string_view>>
        kNoOldVersionIcons({
            kDriveLogoIconResourceName,
            kEnterpriseIconResourceName,
            searchbox_internal::kReplyRotated180IconResourceName,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            kGoogleAgentspaceMonochromeLogoIcon,
            kGoogleAgentspaceMonochromeLogo25Icon,
            kGoogleCalendarIconResourceName,
            kGoogleGIconResourceName,
            kGoogleLensMonochromeLogoIcon,
            kGoogleSitesIconResourceName,
#endif
        });

    if (!kNoOldVersionIcons->contains(resource_name)) {
      base::ReplaceSubstringsAfterOffset(&resource_name, 0, ".svg", "_old.svg");
    }
  }

  return resource_name;
}

searchbox::mojom::AutocompleteResultPtr
SearchboxHandler::CreateAutocompleteResult(
    int32_t query_id,
    const std::u16string& input,
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model,
    const PrefService* prefs,
    const TemplateURLService* turl_service) const {
  return searchbox::mojom::AutocompleteResult::New(
      query_id, result.sequence_id(), input,
      CreateSuggestionGroupsMap(result, prefs, result.suggestion_groups_map()),
      CreateAutocompleteMatches(result, bookmark_model,
                                result.suggestion_groups_map(), turl_service),
      base::UTF8ToUTF16(result.smart_compose_inline_hint()));
}

base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr>
SearchboxHandler::CreateSuggestionGroupsMap(
    const AutocompleteResult& result,
    const PrefService* prefs,
    const omnibox::GroupConfigMap& suggestion_groups_map) const {
  base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr> result_map;
  for (const auto& pair : suggestion_groups_map) {
    std::u16string header =
        autocomplete_controller()->GetSuggestionGroupHeaderText(pair.first);

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
    bookmarks::BookmarkModel* bookmark_model,
    const omnibox::GroupConfigMap& suggestion_groups_map,
    const TemplateURLService* turl_service) const {
  // Tracks whether the first contextual match has been flagged to force show
  // its description, ensuring only the first one gets flagged.
  bool flagged_contextual = false;
  std::vector<searchbox::mojom::AutocompleteMatchPtr> matches;
  for (const auto& match : result) {
    auto mojom_match =
        CreateAutocompleteMatch(match, matches.size(), bookmark_model,
                                suggestion_groups_map, turl_service);
    if (mojom_match) {
      if (!flagged_contextual && ShouldShowFirstContextualDescription() &&
          match.suggestion_group_id ==
              omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH) {
        mojom_match.value()->show_contextual_description = true;
        flagged_contextual = true;
      }
      matches.push_back(std::move(mojom_match.value()));
    }
  }
  return matches;
}

// TODO(b/546186345): Consider extending this behavior to other searchboxes if
// they also need to show the contextual description.
bool SearchboxHandler::ShouldShowFirstContextualDescription() const {
  return false;
}

bool SearchboxHandler::SupportsKeywordMode() const {
  return false;
}

void SearchboxHandler::OverrideIconPaths(
    const AutocompleteMatch& match,
    searchbox::mojom::AutocompleteMatch* mojom_match) const {
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
}

std::optional<searchbox::mojom::AutocompleteMatchPtr>
SearchboxHandler::CreateAutocompleteMatch(
    const AutocompleteMatch& match,
    size_t line,
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
  OverrideIconPaths(match, mojom_match.get());
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
  mojom_match->show_contextual_description = false;
  mojom_match->type = AutocompleteMatchType::ToString(match.type);
  mojom_match->supports_deletion = match.SupportsDeletion();
  mojom_match->is_two_row_suggestion =
      !mojom_match->image_url.empty() ||
      match.type == AutocompleteMatchType::CALCULATOR ||
      match.enterprise_search_aggregator_type ==
          AutocompleteMatch::EnterpriseSearchAggregatorType::PEOPLE;
  if (match.suggest_template) {
    if (match.suggest_template->secondary_text_placement() ==
        omnibox::SuggestTemplateInfo::BELOW_PRIMARY_TEXT) {
      mojom_match->is_two_row_suggestion = true;
    } else if (match.suggest_template->secondary_text_placement() ==
               omnibox::SuggestTemplateInfo::IN_FRONT_OF_PRIMARY_TEXT) {
      mojom_match->is_two_row_suggestion = false;
    }
  }
  if (!match.from_keyword) {
    for (const auto& action : match.actions) {
// TODO(b/544764632): Implement Pedals for Android.
#if BUILDFLAG(IS_ANDROID)
      if (action->ActionId() == OmniboxActionId::PEDAL) {
        continue;
      }
#endif
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
  }
  std::u16string header_text =
      autocomplete_controller()->GetSuggestionGroupHeaderText(
          match.suggestion_group_id);
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

  mojom_match->is_contextual_suggestion = match.IsContextualSearchSuggestion();

  if (match.suggest_template && match.suggest_template->has_fusebox_action()) {
    mojom_match->fusebox_action = fusebox_action::SyncFuseboxActionProtoToMojo(
        match.suggest_template->fusebox_action());
  }

  if (match.suggest_template && match.suggest_template->has_style()) {
    mojom_match->suggest_style = static_cast<searchbox::mojom::SuggestStyle>(
        match.suggest_template->style());
  }

  if (SupportsKeywordMode()) {
    KeywordState keyword_state;
    std::u16string keyword;
    std::u16string keyword_placeholder;
    match.GetKeywordUiState(turl_service,
                            client() && client()->IsHistoryEmbeddingsEnabled(),
                            &keyword_state, &keyword, &keyword_placeholder);

    searchbox::mojom::KeywordType keyword_type;
    bool has_keyword = false;
    if (keyword_state == KeywordState::kKeyword) {
      keyword_type = searchbox::mojom::KeywordType::kInKeyword;
      has_keyword = true;
    } else if (match.HasInstantKeyword(turl_service)) {
      keyword_type = searchbox::mojom::KeywordType::kInstant;
      has_keyword = true;
    } else if (keyword_state == KeywordState::kHint ||
               !match.associated_keyword.empty()) {
      keyword_type = searchbox::mojom::KeywordType::kChip;
      has_keyword = true;
    }

    // Populate `keyword_model`.
    if (has_keyword) {
      auto keyword_model = searchbox::mojom::MatchKeywordModel::New();
      keyword_model->type = keyword_type;
      keyword_model->keyword = base::UTF16ToUTF8(keyword);
      keyword_model->placeholder = base::UTF16ToUTF8(keyword_placeholder);
      const auto names = searchbox::GetKeywordLabelNames(keyword, turl_service);
      keyword_model->chip_hint = base::UTF16ToUTF8(names.full_name);
      keyword_model->chip_a11y =
          l10n_util::GetStringFUTF8(IDS_ACC_KEYWORD_MODE, names.short_name);
      mojom_match->keyword_model = std::move(keyword_model);
    }
  }

  return mojom_match;
}

WindowOpenDisposition SearchboxHandler::ComputeWindowOpenDisposition(
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key,
    bool via_keyboard) {
  return ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
}

SearchboxHandler::SearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_page,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<OmniboxClient> client,
    std::optional<base::TimeDelta> autocomplete_stop_timer_duration)
    : profile_(profile),
      web_contents_(web_contents),
      client_(std::move(client)),
      page_handler_(this, std::move(pending_page_handler)),
      page_(std::move(pending_page)) {
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    if (client_) {
      AutocompleteControllerConfig autocomplete_controller_config{
          .provider_types = AutocompleteClassifier::DefaultOmniboxProviders()};
      if (omnibox::IsWebUIOmniboxPopupEnabled()) {
        autocomplete_controller_config.show_iph_matches = false;
      }
      if (autocomplete_stop_timer_duration.has_value()) {
        autocomplete_controller_config.stop_timer_duration =
            autocomplete_stop_timer_duration.value();
      }
      autocomplete_controller_ = std::make_unique<AutocompleteController>(
          client_->CreateAutocompleteProviderClient(),
          autocomplete_controller_config);

      // Register with emitter.
      if (auto* emitter = client_->GetAutocompleteControllerEmitter()) {
        autocomplete_controller_->AddObserver(emitter);
      }
    }
  } else {
    if (client_) {
      owned_controller_ = std::make_unique<OmniboxController>(
          std::move(client_), autocomplete_stop_timer_duration);
      controller_ = owned_controller_.get();
    }
  }

  if (web_contents_) {
    PermissionPromptObserver::CreateForWebContents(web_contents_);
    PermissionPromptObserver::FromWebContents(web_contents_)->AddObserver(this);
  }

  if (profile_ && profile_->GetPrefs()) {
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(
        omnibox::kKeywordSpaceTriggeringEnabled,
        base::BindRepeating(
            &SearchboxHandler::OnKeywordSpaceTriggeringPrefChanged,
            base::Unretained(this)));
    OnKeywordSpaceTriggeringPrefChanged();
  }
}

SearchboxHandler::~SearchboxHandler() {
  controller_ = nullptr;
  if (web_contents_) {
    if (auto* observer =
            PermissionPromptObserver::FromWebContents(web_contents_)) {
      observer->RemoveObserver(this);
    }
  }
}

void SearchboxHandler::OnKeywordSpaceTriggeringPrefChanged() {
  if (page_) {
    page_->SetKeywordSpaceTriggeringEnabled(profile_->GetPrefs()->GetBoolean(
        omnibox::kKeywordSpaceTriggeringEnabled));
  }
}

void SearchboxHandler::AddFileContextFromBrowser(
    base::UnguessableToken token,
    searchbox::mojom::SelectedFileInfoPtr file_info) {
  page_->AddFileContext(token, std::move(file_info));
}

void SearchboxHandler::OnContextualInputStatusChanged(
    base::UnguessableToken token,
    contextual_search::ContextUploadStatus status,
    std::optional<contextual_search::ContextUploadErrorType> error_type) {
  page_->OnContextualInputStatusChanged(token, status, error_type);
}

void SearchboxHandler::OnScreenshotMenuClosed() {
  page_->OnScreenshotMenuClosed();
}

void SearchboxHandler::OnFocusChanged(bool focused) {
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    metrics_tracker_.FocusChanged(focused);
  } else {
    if (focused) {
      edit_model()->OnSetFocus(false);
    } else {
      edit_model()->OnWillKillFocus();
      edit_model()->OnKillFocus();
    }
  }
}

void SearchboxHandler::QueryAutocomplete(
    int32_t query_id,
    std::optional<int32_t> tab_id,
    const std::u16string& input,
    bool prevent_inline_autocomplete,
    uint32_t cursor_position,
    omnibox::SuggestInventory suggest_inventory,
    bool is_on_focus,
    const std::string& keyword,
    searchbox::mojom::InputMethod input_method) {
  DCHECK(!tab_id.has_value())
      << "QueryAutocomplete with tab_id is only supported for the full WebUI "
         "Omnibox.";

  current_query_id_ = query_id;

  std::u16string input_with_keyword = input;
  bool is_keyword_selected = false;
  const TemplateURL* template_url = nullptr;
  if (!keyword.empty()) {
    TemplateURLService* service =
        client() ? client()->GetTemplateURLService() : nullptr;
    if (service) {
      std::u16string keyword16;
      // TODO(b:504669216): There may actually exist a `TemplateURL` with
      //   shortcut '?'. Using '?' as a sentinel value to represent the default
      //   search engine will incorrectly trigger the default search engine even
      //   when the user wanted the '?' search engine.
      if (keyword == "?") {
        template_url = service->GetDefaultSearchProvider();
        if (template_url) {
          keyword16 = template_url->keyword();
        }
      } else {
        keyword16 = base::UTF8ToUTF16(keyword);
        template_url = service->GetTemplateURLForKeyword(keyword16);
      }
      if (template_url) {
        is_keyword_selected = true;
        input_with_keyword = keyword16 + u" " + input;
        cursor_position += keyword16.length() + 1;
      }
    }
  }

  // This shouldn't happen, but, e.g., users may do unintended actions in the
  // developer console and crashing with a `CHECK()` doesn't seem warranted.
  cursor_position = std::min(
      cursor_position, static_cast<uint32_t>(input_with_keyword.length()));

  // Early exit if a query is already in progress for on focus inputs.
  if (!autocomplete_controller()->done() && is_on_focus) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    if (!is_on_focus) {
      // For non-ZPS input, this will SetInputInProgress and consequently mark
      // the input timer so that Omnibox.TypingDuration will be logged
      // correctly.
      edit_model()->SetUserText(input);
    }
    // There are various `CHECK()`s and assumptions in the `OmniboxEditModel`
    // that verify the keyword state is set. Even though we're relying on
    // searchbox webUI code to manage its keyword state, we need to propagate to
    // `OmniboxEditModel`'s too to avoid crashes and bugs. This won't be
    // necessary as we kill the `OmniboxEditModel`.
    if (is_keyword_selected && template_url) {
      edit_model()->SetKeywordInfo(
          KeywordState::kKeyword, template_url->keyword(),
          /*keyword_placeholder=*/u"",
          keyword == "?" ? metrics::OmniboxEventProto::QUESTION_MARK
                         : metrics::OmniboxEventProto::SPACE_AT_END);
    } else {
      edit_model()->SetKeywordInfo(KeywordState::kNone, u"", u"",
                                   metrics::OmniboxEventProto::INVALID);
    }
  } else if (!is_on_focus &&
             metrics_tracker_.time_user_first_modified_omnibox().is_null()) {
    metrics_tracker_.set_time_user_first_modified_omnibox(
        base::TimeTicks::Now());
  }

  // RealboxOmniboxClient::GetPageClassification() ignores the arguments.
  const auto page_classification =
      client()->GetPageClassification(/*is_prefetch=*/false);
  AutocompleteInput autocomplete_input(
      input_with_keyword, cursor_position, page_classification,
      ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_current_url(client()->GetURL());
  autocomplete_input.set_current_title(client()->GetTitle());
  autocomplete_input.set_focus_type(
      is_on_focus ? metrics::OmniboxFocusType::INTERACTION_FOCUS
                  : metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);
  // TODO(b/504669216): `set_allow_exact_keyword_match()` should be true even
  //   when not in keyword mode.
  autocomplete_input.set_allow_exact_keyword_match(is_keyword_selected);
  autocomplete_input.set_in_keyword_mode(is_keyword_selected);
  // Set the lens overlay suggest inputs, if available.
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          client()->GetLensOverlaySuggestInputs()) {
    // Don't set lens params if in "Create Image" with an image present or in
    // "Canvas" mode. This prevents the contextual client from being used in
    // this tool mode.
    autocomplete_input.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }
  if (client()->GetContextualInputData().has_value()) {
    auto context_data = client()->GetContextualInputData().value();
    if (context_data.page_title.has_value() &&
        context_data.page_url.has_value()) {
      autocomplete_input.set_context_tab_title(
          base::UTF8ToUTF16(context_data.page_title.value()));
      autocomplete_input.set_context_tab_url(context_data.page_url.value());
    }
  }

  autocomplete_input.set_input_state(GetInputState());
  autocomplete_input.set_previous_query(GetPreviousQuery());
  autocomplete_input.set_suggest_inventory(suggest_inventory);
  // TODO(crbug.com/543112749): Support other input methods for Smart Compose.
  autocomplete_input.set_input_method(
      static_cast<omnibox::metrics::ChromeSearchboxStats::InputMethod>(
          input_method));
  autocomplete_input.set_has_previous_submitted_thread_context(
      client()->HasPreviousSubmittedThreadContext());
  autocomplete_input.set_has_auto_suggested_tab(
      client()->HasAutoSuggestedTab());

  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    autocomplete_controller()->Start(autocomplete_input);
  } else {
    edit_model()->SetAutocompleteInput(autocomplete_input);
    omnibox_controller()->StartAutocomplete(autocomplete_input);
  }
}

void SearchboxHandler::StopAutocomplete(bool clear_result) {
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    autocomplete_controller()->Stop(clear_result
                                        ? AutocompleteStopReason::kClobbered
                                        : AutocompleteStopReason::kInteraction);
  } else {
    omnibox_controller()->StopAutocomplete(clear_result);
  }
}

void SearchboxHandler::OpenMatch(OmniboxPopupSelection selection,
                                 AutocompleteMatch match,
                                 WindowOpenDisposition disposition,
                                 base::TimeTicks match_selection_timestamp) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (base::FeatureList::IsEnabled(
          extensions_features::kSearchEngineExplicitChoiceDialog) &&
      AutocompleteMatch::IsSearchType(match.type) && !match.keyword.empty() &&
      match.destination_url.is_valid() &&
      client()->ShowConfirmationDialogIfDefaultSearchExtensionControlled(
          match.destination_url,
          base::BindOnce(&SearchboxHandler::OnDefaultSearchExtensionDialogDone,
                         weak_ptr_factory_.GetWeakPtr(), selection, match,
                         disposition, match_selection_timestamp))) {
    return;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  metrics_tracker_.set_match_selection_timestamp(match_selection_timestamp);
  metrics_tracker_.set_focus_resulted_in_navigation(true);
  // TODO(crbug.com/530254690): Associate inputs and results for match.
  searchbox::OpenMatch(autocomplete_controller(), client(),
                       autocomplete_controller()->input(), selection, match,
                       disposition, metrics_tracker_,
                       metrics::OmniboxEventProto::INVALID, u"");
}

void SearchboxHandler::OpenAutocompleteMatch(
    uint8_t line,
    const GURL& url,
    bool are_matches_showing,
    uint8_t mouse_button,
    searchbox::mojom::ActionModifiersPtr modifiers,
    bool via_keyboard) {
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const WindowOpenDisposition disposition = ComputeWindowOpenDisposition(
      mouse_button, modifiers->alt_key, modifiers->ctrl_key,
      modifiers->meta_key, modifiers->shift_key, via_keyboard);

  if (line == static_cast<uint8_t>(OmniboxPopupSelection::kNoMatch)) {
    const OmniboxPopupSelection selection(OmniboxPopupSelection::kNoMatch);
    // TODO(crbug.com/545723506): Use match from AutocompleteResult.
    if (base::FeatureList::IsEnabled(
            omnibox::kWebUISearchboxWithoutModelController)) {
      AutocompleteMatch verbatim_match;
      searchbox::ClassifyString(
          client(), autocomplete_controller()->input().text(),
          /*in_keyword_mode=*/false,
          /*allow_exact_keyword_match=*/true, &verbatim_match);
      OpenMatch(selection, verbatim_match, disposition, timestamp);
    } else {
      edit_model()->OpenSelection(selection, timestamp, disposition,
                                  via_keyboard);
    }
    return;
  }

  const AutocompleteMatch* match = GetMatchWithUrl(line, url);
  if (!match) {
    // This can happen due to asynchronous updates changing the result while
    // the web UI is referencing a stale match.
    return;
  }
  const OmniboxPopupSelection selection(line);
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    OpenMatch(selection, *match, disposition, timestamp);
  } else {
    edit_model()->OpenSelection(selection, timestamp, disposition,
                                via_keyboard);
  }
}

OmniboxPopupSelection ConvertSelection(
    searchbox::mojom::OmniboxPopupSelectionPtr selection) {
  OmniboxPopupSelection::LineState state =
      OmniboxPopupSelection::LineState::LINE_STATE_MAX_VALUE;
  switch (selection->state) {
    case searchbox::mojom::SelectionLineState::kNormal: {
      state = OmniboxPopupSelection::LineState::NORMAL;
      break;
    }
    case searchbox::mojom::SelectionLineState::kKeywordMode: {
      state = OmniboxPopupSelection::LineState::KEYWORD_MODE;
      break;
    }
    case searchbox::mojom::SelectionLineState::kFocusedButtonAction: {
      state = OmniboxPopupSelection::LineState::FOCUSED_BUTTON_ACTION;
      break;
    }
    case searchbox::mojom::SelectionLineState::kFocusedButtonRemoveSuggestion: {
      state =
          OmniboxPopupSelection::LineState::FOCUSED_BUTTON_REMOVE_SUGGESTION;
      break;
    }
    case searchbox::mojom::SelectionLineState::kFocusedButtonAim: {
      state = OmniboxPopupSelection::LineState::FOCUSED_BUTTON_AIM;
      break;
    }
    case searchbox::mojom::SelectionLineState::
        kFocusedButtonContextEntrypoint: {
      // Handled directly by webui omnibox popup.
      NOTREACHED();
    }
    case searchbox::mojom::SelectionLineState::kCtrlEnter: {
      state = OmniboxPopupSelection::LineState::CTRL_ENTER;
      break;
    }
  }
  CHECK_NE(state, OmniboxPopupSelection::LineState::LINE_STATE_MAX_VALUE);
  // Special case line for mojom equivalent of kNoMatch; it is represented
  // as uint8_t so direct conversion would become a positive out of bounds
  // index.
  return OmniboxPopupSelection(
      selection->line == static_cast<uint8_t>(OmniboxPopupSelection::kNoMatch)
          ? OmniboxPopupSelection::kNoMatch
          : selection->line,
      state, selection->action_index);
}

void SearchboxHandler::SetPopupSelection(
    searchbox::mojom::OmniboxPopupSelectionPtr selection) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    OmniboxPopupSelection popup_selection =
        ConvertSelection(std::move(selection));
    if (popup_selection.line != OmniboxPopupSelection::kNoMatch &&
        popup_selection.line >= autocomplete_controller()->result().size()) {
      return;
    }
    edit_model()->SetPopupSelection(popup_selection, false, false, false);
  }
}

void SearchboxHandler::OpenPopupSelection(
    uint32_t result_sequence_id,
    searchbox::mojom::OmniboxPopupSelectionPtr selection,
    WindowOpenDisposition disposition) {
  const OmniboxPopupSelection popup_selection =
      ConvertSelection(std::move(selection));
  const bool sequence_id_matched =
      result_sequence_id == autocomplete_controller()->result().sequence_id();

  if (!base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    const bool selection_matched =
        popup_selection == edit_model()->GetPopupSelection() ||
        popup_selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_AIM ||
        popup_selection.state == OmniboxPopupSelection::CTRL_ENTER;
    base::UmaHistogramBoolean("Omnibox.WebUI.SelectionMatched",
                              selection_matched);
    base::UmaHistogramBoolean(
        "Omnibox.WebUI.AutocompleteResultSequenceIdMatched",
        sequence_id_matched);

    if ((!selection_matched || !sequence_id_matched) &&
        base::FeatureList::IsEnabled(kDropMismatchedSelections)) {
      return;
    }

    edit_model()->OpenSelection(popup_selection);
    return;
  }

  base::UmaHistogramBoolean("Omnibox.WebUI.AutocompleteResultSequenceIdMatched",
                            sequence_id_matched);

  if (!sequence_id_matched &&
      base::FeatureList::IsEnabled(kDropMismatchedSelections)) {
    return;
  }

  if (popup_selection.line >= autocomplete_controller()->result().size()) {
    return;
  }

  const AutocompleteMatch& match =
      autocomplete_controller()->result().match_at(popup_selection.line);

  if (popup_selection.state == OmniboxPopupSelection::FOCUSED_BUTTON_ACTION) {
    if (popup_selection.action_index < match.actions.size()) {
      auto* action = match.actions[popup_selection.action_index].get();
      if (action) {
        client()->ExecuteAction(
            action, disposition, base::TimeTicks::Now(),
            *(autocomplete_controller()->autocomplete_provider_client()));
      }
    }
  } else if (popup_selection.state == OmniboxPopupSelection::CTRL_ENTER) {
    AutocompleteMatch final_match = match;
    if (autocomplete_controller()->history_url_provider()) {
      std::u16string text_for_tld = autocomplete_controller()->input().text();
      if (popup_selection.line > 0) {
        text_for_tld = match.fill_into_edit;
      }
      AutocompleteMatch alternate_match = searchbox::GenerateDotComMatch(
          client(), autocomplete_controller(),
          autocomplete_controller()->input(), text_for_tld);
      if (alternate_match.destination_url.is_valid()) {
        final_match = alternate_match;
      }
    }
    OpenMatch(popup_selection, final_match, disposition,
              base::TimeTicks::Now());
  } else {
    OpenMatch(popup_selection, match, disposition, base::TimeTicks::Now());
  }
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
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    autocomplete_controller()->Stop(AutocompleteStopReason::kInteraction);
  } else {
    omnibox_controller()->StopAutocomplete(/*clear_result=*/false);
  }
  autocomplete_controller()->DeleteMatch(*match);
}

void SearchboxHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {
  // TODO(b/449785444): Allow embedders other than the Omnibox to activate
  // keyword mode.
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
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    auto* action = match->actions[action_index].get();
    if (action) {
      client()->ExecuteAction(
          action, disposition, match_selection_timestamp,
          *(autocomplete_controller()->autocomplete_provider_client()));
    }
  } else {
    edit_model()->OpenSelection(
        OmniboxPopupSelection(
            line, OmniboxPopupSelection::FOCUSED_BUTTON_ACTION, action_index),
        match_selection_timestamp, disposition);
  }
}

void SearchboxHandler::GetCyclingPlaceholderConfig(
    GetCyclingPlaceholderConfigCallback callback) {
  std::vector<std::u16string> placeholders;

  AimEligibilityService* service =
      AimEligibilityServiceFactory::GetForProfile(profile_);

  // Non-AI-gated: always first per UX spec.
  placeholders.emplace_back(l10n_util::GetStringUTF16(
      IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_ASK_GOOGLE));

  // Evergreen placeholders, gated on AI Mode eligibility only.
  if (service && service->IsAimEligible()) {
    placeholders.emplace_back(l10n_util::GetStringUTF16(
        IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_RESEARCH_TOPIC));
    placeholders.emplace_back(l10n_util::GetStringUTF16(
        IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_LEARN_SKILL));
    placeholders.emplace_back(l10n_util::GetStringUTF16(
        IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_GET_ADVICE));
  }

  // Cycling requires at least 2 texts. If the user is not eligible, clear
  // the placeholders to disable cycling and fall back to the static
  // placeholder text.
  if (placeholders.size() <= 1) {
    placeholders.clear();
  }

  const auto placeholder_config = ntp_composebox::FeatureConfig::Get()
                                      .config.composebox()
                                      .placeholder_config();
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

void SearchboxHandler::WaitForTabFaviconLoad(
    int32_t tab_id,
    WaitForTabFaviconLoadCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void SearchboxHandler::GetInputState(GetInputStateCallback callback) {
  std::move(callback).Run({});
}

void SearchboxHandler::OnResultChanged(AutocompleteController* controller,
                                       bool default_match_changed) {
  TemplateURLService* template_url_service =
      client() ? client()->GetTemplateURLService() : nullptr;

  std::u16string input_text = controller->input().text();
  if (controller->input().in_keyword_mode() && template_url_service) {
    std::u16string keyword;
    std::u16string query;
    if (AutocompleteInput::ExtractKeywordFromInput(
            controller->input(), template_url_service, &keyword, &query)) {
      input_text = query;
    }
  }

  page_->AutocompleteResultChanged(CreateAutocompleteResult(
      current_query_id_, input_text, autocomplete_controller()->result(),
      BookmarkModelFactory::GetForBrowserContext(profile_),
      profile_->GetPrefs(), template_url_service));

  // If the AutocompleteController is owned by the handler, notify the prerender
  // here to start preloading if the results are ready.
  bool should_preload = false;
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    should_preload = !!client_;
  } else {
    should_preload = !!owned_controller_;
  }

  if (should_preload) {
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

void SearchboxHandler::OnControllerDestroying(
    AutocompleteController* controller) {
  if (autocomplete_controller_observation_.IsObservingSource(controller)) {
    autocomplete_controller_observation_.Reset();
  }
}

void SearchboxHandler::OnPermissionPromptChanged(bool is_showing,
                                                 const gfx::Size& prompt_size) {
  gfx::Size size_with_buffer;
  if (is_showing) {
    const int width = prompt_size.width();
    const int height = prompt_size.height();
    // Ensure buffer is not added if height or width is 0.
    size_with_buffer.SetSize(width > 0 ? width + kPromptWidthBuffer : 0,
                             height > 0 ? height + kPromptHeightBuffer : 0);
  }

  page_->OnPermissionPromptChanged(is_showing, size_with_buffer);

  if (omnibox_delegate_) {
    omnibox_delegate_->OnEmbeddedPermissionDialogChanged(is_showing,
                                                         size_with_buffer);
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

omnibox::InputState SearchboxHandler::GetInputState() const {
  return omnibox::InputState();
}

std::string SearchboxHandler::GetPreviousQuery() {
  return std::string();
}

void SearchboxHandler::GetDriveDisclaimerStatus(
    GetDriveDisclaimerStatusCallback callback) {
  std::move(callback).Run(searchbox::mojom::DriveDisclaimerStatus::kRestricted);
}

void SearchboxHandler::OnDriveDisclaimerAccepted() {}

void SearchboxHandler::OnDriveUploadClicked(
    OnDriveUploadClickedCallback callback) {
  std::move(callback).Run(searchbox::mojom::DriveUploadResponse::New());
}

OmniboxController* SearchboxHandler::omnibox_controller() const {
  return controller_;
}

OmniboxClient* SearchboxHandler::client() const {
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController) &&
      client_) {
    return client_.get();
  }
  return controller_ ? controller_->client() : nullptr;
}

AutocompleteController* SearchboxHandler::autocomplete_controller() const {
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController) &&
      autocomplete_controller_) {
    return autocomplete_controller_.get();
  }
  return controller_ ? controller_->autocomplete_controller() : nullptr;
}

OmniboxEditModel* SearchboxHandler::edit_model() const {
  return controller_ ? controller_->edit_model() : nullptr;
}

void SearchboxHandler::SetAutocompleteControllerForTesting(
    std::unique_ptr<AutocompleteController> controller) {
  if (base::FeatureList::IsEnabled(
          omnibox::kWebUISearchboxWithoutModelController)) {
    autocomplete_controller_ = std::move(controller);
  } else if (controller_) {
    controller_->SetAutocompleteControllerForTesting(std::move(controller));
  }
}

void SearchboxHandler::GetPageClassification(
    GetPageClassificationCallback callback) {
  metrics::OmniboxEventProto::PageClassification classification_enum =
      client()->GetPageClassification(/*is_prefetch=*/false);
  std::move(callback).Run(::metrics::OmniboxEventProto::PageClassification_Name(
      classification_enum));
}

void SearchboxHandler::OnDefaultSearchExtensionDialogDone(
    OmniboxPopupSelection selection,
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    base::TimeTicks match_selection_timestamp,
    OmniboxClient::ExtensionControlledDialogResult dialog_result) {
  if (dialog_result ==
          OmniboxClient::ExtensionControlledDialogResult::kAccept ||
      dialog_result ==
          OmniboxClient::ExtensionControlledDialogResult::kNoDialogShown) {
    OpenMatch(selection, match, disposition, match_selection_timestamp);
  } else if (dialog_result ==
             OmniboxClient::ExtensionControlledDialogResult::kReject) {
    std::u16string input_text = autocomplete_controller()->input().text();
    AutocompleteMatch new_match;
    GURL new_alternate_nav_url;

    searchbox::ClassifyString(
        client(), input_text,
        autocomplete_controller()->input().in_keyword_mode(),
        /*allow_exact_keyword_match=*/true, &new_match, &new_alternate_nav_url);

    OpenMatch(selection, new_match, disposition, match_selection_timestamp);
    client()->FocusWebContents();
  }
}

#if !BUILDFLAG(IS_ANDROID)
void SearchboxHandler::SetSmartTabSharingActive(bool active) {}

void SearchboxHandler::GetSmartTabSharingActive(
    GetSmartTabSharingActiveCallback callback) {
  std::move(callback).Run(false);
}
#endif

void SearchboxHandler::StartScreenshare(bool prefer_entire_screen,
                                        StartScreenshareCallback callback) {
  NOTREACHED();
}

void SearchboxHandler::CaptureRegionScreenshot(
    CaptureRegionScreenshotCallback callback) {
  NOTREACHED();
}

OmniboxController* SearchboxHandler::Delegate::GetOmniboxController() {
  return nullptr;
}
