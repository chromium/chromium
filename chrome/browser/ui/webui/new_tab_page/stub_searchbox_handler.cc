// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/stub_searchbox_handler.h"

#include "components/omnibox/common/input_state.h"

#if BUILDFLAG(IS_ANDROID)

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

StubSearchboxHandler::StubSearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver,
    mojo::PendingRemote<searchbox::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

StubSearchboxHandler::~StubSearchboxHandler() = default;

void StubSearchboxHandler::OnFocusChanged(bool focused) {}
void StubSearchboxHandler::OnThumbnailRemoved() {}
void StubSearchboxHandler::QueryAutocomplete(const std::u16string& input,
                                             bool prevent_inline_autocomplete) {
  if (!page_.is_bound()) {
    return;
  }

  auto result = searchbox::mojom::AutocompleteResult::New();
  result->sequence_id = 1;
  result->input = input;

  auto match = searchbox::mojom::AutocompleteMatch::New();
  match->contents = input + u" - Stub result";
  match->description = u"Click to search";
  match->destination_url =
      GURL("https://www.google.com/search?q=" + base::UTF16ToUTF8(input));

  result->matches.push_back(std::move(match));

  page_->AutocompleteResultChanged(std::move(result));
}
void StubSearchboxHandler::StopAutocomplete(bool clear_result) {}
void StubSearchboxHandler::OpenAutocompleteMatch(uint8_t line,
                                                 const GURL& url,
                                                 bool are_matches_showing,
                                                 uint8_t mouse_button,
                                                 bool alt_key,
                                                 bool ctrl_key,
                                                 bool meta_key,
                                                 bool shift_key) {}
void StubSearchboxHandler::SetPopupSelection(
    searchbox::mojom::OmniboxPopupSelectionPtr selection) {}
void StubSearchboxHandler::OpenPopupSelection(
    uint32_t result_sequence_id,
    searchbox::mojom::OmniboxPopupSelectionPtr selection,
    WindowOpenDisposition disposition) {}
void StubSearchboxHandler::OnNavigationLikely(
    uint8_t line,
    const GURL& url,
    omnibox::mojom::NavigationPredictor navigation_predictor) {}
void StubSearchboxHandler::DeleteAutocompleteMatch(uint8_t line,
                                                   const GURL& url) {}
void StubSearchboxHandler::ActivateKeyword(
    uint8_t line,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    bool is_mouse_event) {}
void StubSearchboxHandler::ExecuteAction(
    uint8_t line,
    uint8_t action_index,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {}
void StubSearchboxHandler::GetPlaceholderConfig(
    GetPlaceholderConfigCallback callback) {
  auto config = searchbox::mojom::PlaceholderConfig::New();
  config->texts.push_back(u"Ask anything...");
  config->change_text_animation_interval = base::Seconds(5);
  config->fade_text_animation_duration = base::Milliseconds(500);
  std::move(callback).Run(std::move(config));
}
void StubSearchboxHandler::GetRecentTabs(GetRecentTabsCallback callback) {
  std::move(callback).Run({});
}
void StubSearchboxHandler::GetTabPreview(int32_t tab_id,
                                         GetTabPreviewCallback callback) {
  std::move(callback).Run({});
}
void StubSearchboxHandler::GetInputState(GetInputStateCallback callback) {
  omnibox::InputState state;
  state.allowed_input_types.push_back(
      omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  state.allowed_input_types.push_back(omnibox::InputType::INPUT_TYPE_LENS_FILE);
  state.allowed_tools.push_back(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  std::move(callback).Run(std::move(state));
}
void StubSearchboxHandler::NotifySessionStarted() {}
void StubSearchboxHandler::NotifySessionAbandoned() {}
void StubSearchboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  std::move(callback).Run({});
}
void StubSearchboxHandler::SetSmartComposeStats(
    searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) {}
void StubSearchboxHandler::AddTabContext(int32_t tab_id,
                                         bool delay_upload,
                                         AddTabContextCallback callback) {
  std::move(callback).Run({});
}
void StubSearchboxHandler::OnDriveUploadClicked(
    OnDriveUploadClickedCallback callback) {
  std::move(callback).Run({});
}
void StubSearchboxHandler::DeleteContext(
    const base::UnguessableToken& file_token,
    bool from_automatic_chip) {}
void StubSearchboxHandler::ClearFiles(bool should_block_auto_suggested_tabs) {}
void StubSearchboxHandler::SubmitQuery(const std::string& query_text,
                                       uint8_t mouse_button,
                                       bool alt_key,
                                       bool ctrl_key,
                                       bool meta_key,
                                       bool shift_key) {}
void StubSearchboxHandler::OpenLensSearch() {}
void StubSearchboxHandler::SetActiveToolMode(omnibox::ToolMode tool) {}
void StubSearchboxHandler::RecordToolSelectionAction(omnibox::ToolMode tool) {}
void StubSearchboxHandler::SetActiveModelMode(omnibox::ModelMode model) {}
void StubSearchboxHandler::RecordModelSelectionAction(
    omnibox::ModelMode model) {}
void StubSearchboxHandler::ActivateMetricsFunnel(
    const std::string& funnel_name) {}
void StubSearchboxHandler::ShouldShowDriveDisclaimer(
    ShouldShowDriveDisclaimerCallback callback) {
  std::move(callback).Run(false);
}
void StubSearchboxHandler::OnDriveDisclaimerAccepted() {}
void StubSearchboxHandler::GetPageClassification(
    GetPageClassificationCallback callback) {
  std::move(callback).Run({});
}

// static
void StubSearchboxHandler::SetupWebUIDataSource(
    content::WebUIDataSource* source,
    Profile* profile) {
  source->AddBoolean("isTopChromeSearchbox", false);
  source->AddBoolean("isLensSearchbox", false);
  source->AddBoolean("reportMetrics", false);
  source->AddString("charTypedToPaintMetricName", "");
  source->AddString("resultChangedToPaintMetricName", "");
  source->AddBoolean("forceHideEllipsis", false);
  source->AddBoolean("enableThumbnailSizingTweaks", false);
  source->AddBoolean("enableCsbMotionTweaks", false);

  static constexpr webui::LocalizedString kSearchboxStrings[] = {
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
      {"dismissButton", IDS_NTP_DISMISS},
      {"lensSearchLabel", IDS_WEBUI_OMNIBOX_COMPOSE_LENS_OVERLAY},
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
      {"askAboutPreviousTab", IDS_COMPOSE_ASK_ABOUT_THIS_TAB},
      {"askAboutPreviousTabAriaLabel",
       IDS_COMPOSE_ASK_ABOUT_THIS_TAB_ARIA_LABEL},
      {"removeToolChipAriaLabel", IDS_COMPOSE_REMOVE_TOOL_CHIP_A11Y_LABEL},
      {"composeFileTypesAllowedError",
       IDS_NTP_COMPOSE_FILE_TYPE_NOT_ALLOWED_ERROR},
      {"voiceClose", IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP},
      {"voiceDetails", IDS_NEW_TAB_VOICE_DETAILS},
      {"voiceListening", IDS_NEW_TAB_VOICE_LISTENING},
      {"voicePermissionError", IDS_NEW_TAB_VOICE_PERMISSION_ERROR},
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
  source->AddLocalizedStrings(kSearchboxStrings);

  source->AddString(
      "composeboxDragAndDropHint",
      l10n_util::GetPluralStringFUTF16(IDS_NTP_COMPOSE_DRAG_AND_DROP_HINT, 10));

  source->AddBoolean("searchboxVoiceSearch", true);
  source->AddBoolean("searchboxLensSearch", false);
  source->AddString("searchboxLensVariations", "");
  source->AddBoolean(
      "searchboxCr23Theming",
      base::FeatureList::IsEnabled(ntp_features::kRealboxCr23Theming));
  source->AddBoolean("searchboxCr23SteadyStateShadow",
                     ntp_features::kNtpRealboxCr23SteadyStateShadow.Get());
  source->AddString("searchboxDefaultIcon",
                    "//resources/images/icon_search.svg");

  // Composebox stuff (if needed by JS)
  source->AddBoolean("ntpRealboxNextEnabled",
                     ntp_realbox::IsNtpRealboxNextEnabled(profile));
  source->AddBoolean("searchboxShowComposeAnimation", false);
  source->AddInteger("composeboxFileMaxCount", 10);
  source->AddString("composeboxPlaceholderText", "");
  source->AddString("searchboxComposePlaceholder", "");
  source->AddString("suggestionActivityLink", "");
  source->AddBoolean("composeboxContextDragAndDropEnabled", false);
  source->AddBoolean("contextualMenuUsePecApi", false);
  source->AddBoolean("ShowContextMenuHeaders", false);
  source->AddBoolean("thinkingModelIconUpdate", false);
}

#endif  // BUILDFLAG(IS_ANDROID)
