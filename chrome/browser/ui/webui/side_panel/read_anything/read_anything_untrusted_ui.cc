// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_read_anything_resources.h"
#include "chrome/grit/side_panel_read_anything_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/views/style/platform_style.h"

ReadAnythingUIUntrustedConfig::ReadAnythingUIUntrustedConfig()
    : DefaultTopChromeWebUIConfig(
          content::kChromeUIUntrustedScheme,
          chrome::kChromeUIUntrustedReadAnythingSidePanelHost) {}

ReadAnythingUIUntrustedConfig::~ReadAnythingUIUntrustedConfig() = default;

ReadAnythingUntrustedUI::ReadAnythingUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedReadAnythingSidePanelURL);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"readAnythingTabTitle", IDS_READING_MODE_TITLE},
      {"notSelectableHeader", IDS_READING_MODE_NOT_SELECTABLE_HEADER},
      {"emptyStateHeader", IDS_READING_MODE_EMPTY_STATE_HEADER},
      {"emptyStateSubheader", IDS_READING_MODE_EMPTY_STATE_SUBHEADER},
      {"readAnythingLoadingMessage", IDS_READ_ANYTHING_LOADING},
      {"lineSpacingTitle", IDS_READING_MODE_LINE_SPACING_COMBOBOX_LABEL},
      {"fontNameTitle", IDS_READING_MODE_FONT_NAME_COMBOBOX_LABEL},
      {"themeTitle", IDS_READING_MODE_COLORS_COMBOBOX_LABEL},
      {"letterSpacingTitle", IDS_READING_MODE_LETTER_SPACING_COMBOBOX_LABEL},
      {"fontSizeTitle", IDS_READING_MODE_FONT_SIZE},
      {"defaultColorTitle", IDS_READING_MODE_DEFAULT_COLOR_LABEL},
      {"lightColorTitle", IDS_READING_MODE_LIGHT_COLOR_LABEL},
      {"darkColorTitle", IDS_READING_MODE_DARK_COLOR_LABEL},
      {"yellowColorTitle", IDS_READING_MODE_YELLOW_COLOR_LABEL},
      {"blueColorTitle", IDS_READING_MODE_BLUE_COLOR_LABEL},
      {"fontResetTitle", IDS_READING_MODE_FONT_RESET},
      {"autoHighlightTitle", IDS_READING_MODE_AUTO_HIGHLIGHT_LABEL},
      {"wordHighlightTitle", IDS_READING_MODE_WORD_HIGHLIGHT_LABEL},
      {"phraseHighlightTitle", IDS_READING_MODE_PHRASE_HIGHLIGHT_LABEL},
      {"sentenceHighlightTitle", IDS_READING_MODE_SENTENCE_HIGHLIGHT_LABEL},
      {"noHighlightTitle", IDS_READING_MODE_OFF_HIGHLIGHT_LABEL},
      {"turnHighlightOff", IDS_READING_MODE_TURN_HIGHLIGHT_OFF},
      {"turnHighlightOn", IDS_READING_MODE_TURN_HIGHLIGHT_ON},
      {"lineSpacingStandardTitle", IDS_READING_MODE_SPACING_COMBOBOX_STANDARD},
      {"lineSpacingLooseTitle", IDS_READING_MODE_SPACING_COMBOBOX_LOOSE},
      {"lineSpacingVeryLooseTitle",
       IDS_READING_MODE_SPACING_COMBOBOX_VERY_LOOSE},
      {"letterSpacingStandardTitle",
       IDS_READING_MODE_SPACING_COMBOBOX_STANDARD},
      {"letterSpacingWideTitle", IDS_READING_MODE_SPACING_COMBOBOX_WIDE},
      {"letterSpacingVeryWideTitle",
       IDS_READING_MODE_SPACING_COMBOBOX_VERY_WIDE},
      {"playDescription", IDS_READING_MODE_PLAY_DESCRIPTION},
      {"playAriaLabel", IDS_READING_MODE_PLAY_SPEECH},
      {"stopLabel", IDS_READING_MODE_STOP_SPEECH},
      {"playTooltip", IDS_READING_MODE_PLAY_TOOLTIP},
      {"previewTooltip", IDS_READING_MODE_PREVIEW_TOOLTIP},
      {"pauseTooltip", IDS_READING_MODE_PAUSE_TOOLTIP},
      {"previousSentenceLabel", IDS_READING_MODE_NAVIGATE_PREVIOUS_SENTENCE},
      {"nextSentenceLabel", IDS_READING_MODE_NAVIGATE_NEXT_SENTENCE},
      {"moreOptionsLabel", IDS_READING_MODE_MORE_OPTIONS},
      {"voiceSpeedLabel", IDS_READING_MODE_VOICE_SPEED},
      {"voiceHighlightLabel", IDS_READING_MODE_VOICE_HIGHLIGHT},
      {"voiceSpeedWithRateLabel", IDS_READING_MODE_VOICE_SPEED_WITH_RATE},
      {"voiceSelectionLabel", IDS_READING_MODE_VOICE_SELECTION},
      {"systemVoiceLabel", IDS_READING_MODE_SYSTEM_VOICE},
      {"increaseFontSizeLabel",
       IDS_READING_MODE_INCREASE_FONT_SIZE_BUTTON_LABEL},
      {"decreaseFontSizeLabel",
       IDS_READING_MODE_DECREASE_FONT_SIZE_BUTTON_LABEL},
      {"disableLinksLabel", IDS_READING_MODE_DISABLE_LINKS_BUTTON_LABEL},
      {"enableLinksLabel", IDS_READING_MODE_ENABLE_LINKS_BUTTON_LABEL},
      {"disableImagesLabel", IDS_READING_MODE_DISABLE_IMAGES_BUTTON_LABEL},
      {"enableImagesLabel", IDS_READING_MODE_ENABLE_IMAGES_BUTTON_LABEL},
      {"readingModeToolbarLabel", IDS_READING_MODE_TOOLBAR_LABEL},
      {"readingModeReadAloudToolbarLabel",
       IDS_READING_MODE_READ_ALOUD_TOOLBAR_LABEL},
      {"readingModeVoicePreviewText", IDS_READING_MODE_VOICE_PREVIEW_STRING},
      {"readingModeVoiceMenuDownloading",
       IDS_READING_MODE_VOICE_MENU_DOWNLOADING},
      {"readingModeFontLoadingText", IDS_READING_MODE_FONT_LOADING_STRING},
      {"readingModeLanguageMenu", IDS_READING_MODE_LANGUAGE_MENU},
      {"readingModeLanguageMenuTitle", IDS_READING_MODE_LANGUAGE_MENU_TITLE},
      {"readingModeLanguageMenuClose", IDS_READING_MODE_LANGUAGE_MENU_CLOSE},
      {"readingModeLanguageMenuSearchLabel",
       IDS_READING_MODE_LANGUAGE_MENU_SEARCH_LABEL},
      {"readingModeLanguageMenuSearchClear",
       IDS_READING_MODE_LANGUAGE_MENU_SEARCH_CLEAR},
      {"readingModeLanguageMenuDownloading",
       IDS_READING_MODE_LANGUAGE_MENU_DOWNLOADING},
      {"readingModeLanguageMenuVoicesUnavailable",
       IDS_READING_MODE_LANGUAGE_MENU_VOICES_UNAVAILABLE},
      {"readingModeLanguageMenuNoInternet",
       IDS_READING_MODE_LANGUAGE_MENU_NO_INTERNET},
      {"readingModeVoiceMenuNoInternet",
       IDS_READING_MODE_VOICE_MENU_NO_INTERNET},
      {"readingModeLanguageMenuNoSpace",
       IDS_READING_MODE_LANGUAGE_MENU_NO_SPACE},
      {"readingModeLanguageMenuNoSpaceButVoicesExist",
       IDS_READING_MODE_LANGUAGE_MENU_NO_SPACE_BUT_VOICES_EXIST},
      {"readingModeVoiceMenuNoSpace", IDS_READING_MODE_VOICE_MENU_NO_SPACE},
      {"previewVoiceAccessibilityLabel",
       IDS_READING_MODE_VOICE_MENU_PREVIEW_LANGUAGE},
      {"languageMenuNoResults", IDS_READING_MODE_LANGUAGE_MENU_NO_RESULTS},
      {"readingModeVoiceDownloadedTitle",
       IDS_READING_MODE_VOICE_DOWNLOADED_TITLE},
      {"readingModeVoiceDownloadedMessage",
       IDS_READING_MODE_VOICE_DOWNLOADED_MESSAGE},
      {"menu", IDS_MENU},
      {"selected", IDS_READING_MODE_ITEM_SELECTED},
      {"allocationError", IDS_READING_MODE_LANGUAGE_MENU_NO_SPACE},
      {"allocationErrorHighQuality",
       IDS_READING_MODE_LANGUAGE_MENU_NO_SPACE_BUT_VOICES_EXIST},
      {"allocationErrorNoVoices", IDS_READING_MODE_TOAST_NO_SPACE},
      {"languageMenuDownloadFailed",
       IDS_READING_MODE_LANGUAGE_MENU_DOWNLOAD_FAILED},
      {"cantUseReadAloud", IDS_READING_MODE_CANT_USE_READ_ALOUD},
  };
  for (const auto& str : kLocalizedStrings) {
    webui::AddLocalizedString(source, str.name, str.id);
  }

  // Rather than call `webui::SetupWebUIDataSource`, manually set up source
  // here. This ensures that if CSPs change in a way that is safe for chrome://
  // but not chrome-untrusted://, ReadAnythingUntrustedUI does not inherit them.
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  webui::EnableTrustedTypesCSP(source);
  source->AddResourcePaths(base::make_span(
      kSidePanelReadAnythingResources, kSidePanelReadAnythingResourcesSize));
  source->AddResourcePath("", IDR_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' chrome-untrusted://resources "
      "chrome-untrusted://webui-test;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome-untrusted://resources chrome-untrusted://theme "
      "https://fonts.googleapis.com 'unsafe-inline';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc,
      "font-src 'self' chrome-untrusted://resources "
      "https://fonts.gstatic.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' data: chrome-untrusted://resources;");
  raw_ptr<Profile> profile = Profile::FromWebUI(web_ui);

  // If the ThemeSource isn't added here, since Read Anything is
  // chrome-untrusted, it will be unable to load stylesheets until a new tab
  // is opened.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(
                                           profile, /*serve_untrusted=*/true));
}

ReadAnythingUntrustedUI::~ReadAnythingUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadAnythingUntrustedUI)

void ReadAnythingUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ReadAnythingUntrustedUI::BindInterface(
    mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandlerFactory>
        receiver) {
  read_anything_page_factory_receiver_.reset();
  read_anything_page_factory_receiver_.Bind(std::move(receiver));
}

void ReadAnythingUntrustedUI::CreateUntrustedPageHandler(
    mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
    mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>
        receiver) {
  DCHECK(page);
  read_anything_untrusted_page_handler_ =
      std::make_unique<ReadAnythingUntrustedPageHandler>(
          std::move(page), std::move(receiver), web_ui());

  // This code is called as part of a screen2x data generation workflow, where
  // the browser is opened by a CLI and the read-anything side panel is
  // automatically opened. Therefore we force the UI to show right away rather
  // than waiting for all UI artifacts to load, as in the general case.
  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    if (embedder()) {
      embedder()->ShowUI();
    }
  }
}

void ReadAnythingUntrustedUI::ShouldShowUI() {
  // Show the UI after the Side Panel content has loaded.
  if (embedder()) {
    embedder()->ShowUI();
  }
}
