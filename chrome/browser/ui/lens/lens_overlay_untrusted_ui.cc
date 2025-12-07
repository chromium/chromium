// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/lens_shared_resources.h"
#include "chrome/grit/lens_shared_resources_map.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "components/lens/lens_features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"

namespace lens {

// The number of times to show cursor tooltips.
constexpr int kNumTimesToShowCursorTooltips = 5;
// Time to wait for Lens text response before displaying the selected region
// context menu, in milliseconds.
constexpr int kTextReceivedTimeoutMs = 2000;
// Time to wait for text in the interaction response before falling back to
// using the full image response to copy text from a region.
constexpr int kCopyTextTimeoutMs = 500;
// Time to wait for text in the interaction response before falling back to
// using the full image response to translate text from a region.
constexpr int kTranslateTextTimeoutMs = 500;

LensOverlayUntrustedUI::LensOverlayUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  // Set up the chrome-untrusted://lens-overlay source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUILensOverlayUntrustedURL);
  html_source->AddLocalizedString("backButton", IDS_ACCNAME_BACK);
  html_source->AddLocalizedString("close", IDS_CLOSE);
  html_source->AddLocalizedString("copy", IDS_LENS_OVERLAY_COPY);
  html_source->AddLocalizedString("copyAsImage",
                                  IDS_LENS_OVERLAY_COPY_AS_IMAGE);
  html_source->AddLocalizedString("copyAsImageToastMessage",
                                  IDS_LENS_OVERLAY_COPY_AS_IMAGE_TOAST_MESSAGE);
  html_source->AddLocalizedString("copyText", IDS_LENS_OVERLAY_COPY_TEXT);
  html_source->AddLocalizedString("copyToastMessage",
                                  IDS_LENS_OVERLAY_COPY_TOAST_MESSAGE);
  html_source->AddLocalizedString("dismiss",
                                  IDS_LENS_OVERLAY_TOAST_DISMISS_MESSAGE);
  html_source->AddLocalizedString("learnMore", IDS_LENS_OVERLAY_LEARN_MORE);
  html_source->AddLocalizedString("moreOptions",
                                  IDS_LENS_OVERLAY_MORE_OPTIONS_BUTTON_LABEL);
  html_source->AddLocalizedString("myActivity", IDS_LENS_OVERLAY_MY_ACTIVITY);
  html_source->AddLocalizedString("saveAsImage",
                                  IDS_LENS_OVERLAY_SAVE_AS_IMAGE);
  html_source->AddLocalizedString("sendFeedback", IDS_LENS_SEND_FEEDBACK);
  html_source->AddLocalizedString("cursorTooltipDragMessage",
                                  IDS_LENS_OVERLAY_CURSOR_TOOLTIP_DRAG_MESSAGE);
  html_source->AddLocalizedString(
      "cursorTooltipTextHighlightMessage",
      IDS_LENS_OVERLAY_CURSOR_TOOLTIP_TEXT_HIGHLIGHT_MESSAGE);
  html_source->AddLocalizedString(
      "cursorTooltipClickMessage",
      IDS_LENS_OVERLAY_CURSOR_TOOLTIP_CLICK_MESSAGE);
  html_source->AddLocalizedString(
      "cursorTooltipLivePageMessage",
      IDS_LENS_OVERLAY_CURSOR_TOOLTIP_LIVE_PAGE_MESSAGE);
  html_source->AddLocalizedString("translate", IDS_LENS_OVERLAY_TRANSLATE);
  html_source->AddLocalizedString("translateButtonLabel",
                                  IDS_LENS_OVERLAY_TRANSLATE_BUTTON_LABEL);
  html_source->AddLocalizedString(
      "searchScreenshot",
      IDS_LENS_OVERLAY_SEARCH_SCREENSHOT_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString("selectText", IDS_LENS_OVERLAY_SELECT_TEXT);
  html_source->AddLocalizedString(
      "networkErrorPageTopLine",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_FIRST_LINE);
  html_source->AddLocalizedString(
      "networkErrorPageBottomLine",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_SECOND_LINE);
  html_source->AddLocalizedString(
      "protectedErrorPageTopLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_FIRST_LINE);
  html_source->AddLocalizedString(
      "protectedErrorPageBottomLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_SECOND_LINE);
  html_source->AddLocalizedString("detectLanguage",
                                  IDS_LENS_OVERLAY_DETECT_LANGUAGE_LABEL);
  html_source->AddLocalizedString(
      "translateFrom", IDS_LENS_OVERLAY_SOURCE_LANGUAGE_PICKER_MENU_TITLE);
  html_source->AddLocalizedString(
      "translateTo", IDS_LENS_OVERLAY_TARGET_LANGUAGE_PICKER_MENU_TITLE);
  html_source->AddLocalizedString("allLanguages",
                                  IDS_LENS_OVERLAY_ALL_LANGUAGES_LABEL);
  html_source->AddLocalizedString("recentLanguages",
                                  IDS_LENS_OVERLAY_RECENT_LANGUAGES_LABEL);
  html_source->AddLocalizedString("languagePickerAriaLabel",
                                  IDS_LENS_OVERLAY_LANGUAGE_PICKER_LABEL);
  html_source->AddLocalizedString(
      "translateCloseAriaLabel", IDS_LENS_OVERLAY_CLOSE_TRANSLATE_SCREEN_LABEL);
  html_source->AddLocalizedString(
      "sourceLanguageAriaLabel",
      IDS_LENS_OVERLAY_SOURCE_LANGUAGE_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "targetLanguageAriaLabel",
      IDS_LENS_OVERLAY_TARGET_LANGUAGE_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "searchboxGhostLoaderHintTextPrimaryDefault",
      lens::features::ShouldUseAltLoadingHintWeb()
          ? IDS_GOOGLE_SEARCH_BOX_CONTEXTUAL_LOADING_HINT_PRIMARY_ALT
          : IDS_GOOGLE_SEARCH_BOX_CONTEXTUAL_LOADING_HINT_PRIMARY);
  html_source->AddLocalizedString(
      "searchboxGhostLoaderHintTextPrimaryPdf",
      lens::features::ShouldUseAltLoadingHintPdf()
          ? IDS_GOOGLE_SEARCH_BOX_CONTEXTUAL_LOADING_HINT_PRIMARY_ALT
          : IDS_GOOGLE_SEARCH_BOX_CONTEXTUAL_LOADING_HINT_PRIMARY_PDF);
  html_source->AddLocalizedString(
      "searchboxGhostLoaderHintTextSecondary",
      IDS_GOOGLE_SEARCH_BOX_CONTEXTUAL_LOADING_HINT_SECONDARY);
  html_source->AddLocalizedString("searchboxGhostLoaderErrorText",
                                  IDS_GOOGLE_SEARCH_BOX_CONTEXTUAL_ERROR_TEXT);
  html_source->AddLocalizedString(
      "searchboxGhostLoaderNoSuggestText",
      IDS_GOOGLE_SEARCH_BOX_CONTEXTUAL_NO_SUGGEST_TEXT);
  html_source->AddLocalizedString(
      "searchButton", IDS_LENS_OVERLAY_SEARCH_LANGUAGE_PICKER_LABEL);
  html_source->AddLocalizedString(
      "topLeftSliderAriaLabel",
      IDS_LENS_OVERLAY_TOP_LEFT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "topRightSliderAriaLabel",
      IDS_LENS_OVERLAY_TOP_RIGHT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "bottomRightSliderAriaLabel",
      IDS_LENS_OVERLAY_BOTTOM_RIGHT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "bottomLeftSliderAriaLabel",
      IDS_LENS_OVERLAY_BOTTOM_LEFT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString("privacyNoticeHeader",
                                  IDS_LENS_PERMISSION_BUBBLE_DIALOG_TITLE);
  html_source->AddString(
      "privacyNoticeBody",
      l10n_util::GetStringFUTF16(
          IDS_LENS_PERMISSION_BUBBLE_DIALOG_CSB_DESCRIPTION,
          base::StrCat(
              {u"<a href=\"#\" on-click=\"onLearnMoreClick\" "
               u"on-keydown=\"onLearnMoreClick\" aria-label=\"",
               l10n_util::GetStringUTF16(
                   IDS_LENS_PERMISSION_BUBBLE_DIALOG_LEARN_MORE_ABOUT_GOOGLE_LENS_LINK),
               u"\">", l10n_util::GetStringUTF16(IDS_LENS_OVERLAY_LEARN_MORE),
               u"</a>"})));
  html_source->AddLocalizedString(
      "tabToContinue", IDS_LENS_PERMISSION_BUBBLE_DIALOG_TAB_TO_CONTINUE);

  // Add default theme colors.
  const auto& palette = lens::kPaletteColors.at(lens::PaletteId::kFallback);
  html_source->AddInteger("colorFallbackPrimary",
                          palette.at(lens::ColorId::kPrimary));
  html_source->AddInteger("colorFallbackShaderLayer1",
                          palette.at(lens::ColorId::kShaderLayer1));
  html_source->AddInteger("colorFallbackShaderLayer2",
                          palette.at(lens::ColorId::kShaderLayer2));
  html_source->AddInteger("colorFallbackShaderLayer3",
                          palette.at(lens::ColorId::kShaderLayer3));
  html_source->AddInteger("colorFallbackShaderLayer4",
                          palette.at(lens::ColorId::kShaderLayer4));
  html_source->AddInteger("colorFallbackShaderLayer5",
                          palette.at(lens::ColorId::kShaderLayer5));
  html_source->AddInteger("colorFallbackScrim",
                          palette.at(lens::ColorId::kScrim));
  html_source->AddInteger(
      "colorFallbackSurfaceContainerHighestLight",
      palette.at(lens::ColorId::kSurfaceContainerHighestLight));
  html_source->AddInteger(
      "colorFallbackSurfaceContainerHighestDark",
      palette.at(lens::ColorId::kSurfaceContainerHighestDark));
  html_source->AddInteger("colorFallbackSelectionElement",
                          palette.at(lens::ColorId::kSelectionElement));

  // Add finch flags
  html_source->AddBoolean("enableDebuggingMode",
                          lens::features::IsLensOverlayDebuggingEnabled());
  html_source->AddBoolean("enableShimmer",
                          lens::features::IsLensOverlayShimmerEnabled());
  html_source->AddBoolean(
      "enableShimmerSparkles",
      lens::features::IsLensOverlayShimmerSparklesEnabled());
  html_source->AddInteger("verticalTextMarginPx",
                          lens::features::GetLensOverlayVerticalTextMargin());
  html_source->AddInteger("horizontalTextMarginPx",
                          lens::features::GetLensOverlayHorizontalTextMargin());
  html_source->AddInteger("tapRegionHeight",
                          lens::features::GetLensOverlayTapRegionHeight());
  html_source->AddInteger("tapRegionWidth",
                          lens::features::GetLensOverlayTapRegionWidth());
  html_source->AddDouble(
      "selectTextTriggerThreshold",
      lens::features::GetLensOverlaySelectTextOverRegionTriggerThreshold());
  html_source->AddDouble(
      "postSelectionComparisonThreshold",
      lens::features::GetLensOverlayPostSelectionComparisonThreshold());
  html_source->AddInteger(
      "segmentationMaskCornerRadius",
      lens::features::GetLensOverlaySegmentationMaskCornerRadius());
  html_source->AddBoolean(
      "enableOverlayTranslateButton",
      lens::features::IsLensOverlayTranslateButtonEnabled());
  html_source->AddBoolean("enableCopyAsImage",
                          lens::features::IsLensOverlayCopyAsImageEnabled());
  html_source->AddBoolean("enableSaveAsImage",
                          lens::features::IsLensOverlaySaveAsImageEnabled());
  html_source->AddInteger("textReceivedTimeout", kTextReceivedTimeoutMs);
  html_source->AddInteger("copyTextTimeout", kCopyTextTimeoutMs);
  html_source->AddInteger("translateTextTimeout", kTranslateTextTimeoutMs);
  html_source->AddBoolean(
      "darkMode",
      lens::LensOverlayShouldUseDarkMode(
          ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui))));
  html_source->AddBoolean("enableOverlayContextualSearchbox",
                          lens::IsLensOverlayContextualSearchboxEnabled());
  html_source->AddBoolean(
      "enableGhostLoader",
      lens::features::EnableContextualSearchboxGhostLoader());
  html_source->AddBoolean(
      "showContextualSearchboxLoadingState",
      lens::features::ShowContextualSearchboxGhostLoaderLoadingState());
  html_source->AddBoolean(
      "shouldFetchSupportedLanguages",
      lens::features::IsLensOverlayTranslateLanguagesFetchEnabled());
  html_source->AddString(
      "translateSourceLanguages",
      lens::features::GetLensOverlayTranslateSourceLanguages());
  html_source->AddString(
      "translateTargetLanguages",
      lens::features::GetLensOverlayTranslateSourceLanguages());
  html_source->AddDouble(
      "languagesCacheTimeout",
      lens::features::GetLensOverlaySupportedLanguagesCacheTimeoutMs()
          .InMilliseconds());
  html_source->AddInteger(
      "recentLanguagesAmount",
      lens::features::GetLensOverlayTranslateRecentLanguagesAmount());
  html_source->AddBoolean(
      "enableBorderGlow",
      lens::features::GetVisualSelectionUpdatesEnableBorderGlow());
  html_source->AddBoolean(
      "enableGradientRegionStroke",
      lens::features::GetVisualSelectionUpdatesEnableGradientRegionStroke());
  html_source->AddBoolean(
      "enableWhiteRegionStroke",
      lens::features::GetVisualSelectionUpdatesEnableWhiteRegionStroke());
  html_source->AddBoolean(
      "enableRegionSelectedGlow",
      lens::features::GetVisualSelectionUpdatesEnableRegionSelectedGlow());
  html_source->AddBoolean("autoFocusSearchbox",
                          lens::features::ShouldAutoFocusSearchbox());
  html_source->AddBoolean("cornerSlidersEnabled",
                          lens::features::AreLensOverlayCornerSlidersEnabled());
  html_source->AddInteger("sliderChangedTimeout",
                          lens::features::GetLensOverlaySliderChangedTimeout());
  html_source->AddBoolean(
      "enableCloseButtonTweaks",
      lens::features::GetVisualSelectionUpdatesEnableCloseButtonTweaks());
  html_source->AddBoolean(
      "enableSummarizeSuggestionHint",
      lens::features::ShouldEnableSummarizeHintForContextualSuggest());
  html_source->AddBoolean(
      "enableKeyboardSelection",
      lens::features::IsLensOverlayKeyboardSelectionEnabled());

  LensOverlayController& controller = GetLensOverlayController();
  html_source->AddDouble("invocationTime",
                         controller.GetInvocationTimeSinceEpoch());
  html_source->AddString("invocationSource",
                         controller.GetInvocationSourceString());

  // Allow FrameSrc from all Google subdomains as redirects can occur.
  GURL results_side_panel_url =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  std::string frame_src_directive =
      base::StrCat({"frame-src https://*.google.com ",
                    results_side_panel_url.GetWithEmptyPath().spec(), ";"});
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, frame_src_directive);

  // Allow ImgSrc and StyleSrc from chrome-untrusted:// paths for searchbox use.
  // Allow data URLs to load in WebUI for full page screenshot.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' chrome-untrusted://resources "
      "https://www.gstatic.com data: blob:;");
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome-untrusted://resources chrome-untrusted://theme");

  // Add required resources.
  webui::SetupWebUIDataSource(html_source, kLensUntrustedResources,
                              IDR_LENS_UNTRUSTED_LENS_OVERLAY_HTML);

  html_source->AddResourcePaths(kLensSharedResources);

  // Add required resources for the searchbox.
  SearchboxHandler::SetupWebUIDataSource(html_source,
                                         Profile::FromWebUI(web_ui));
  html_source->AddString(
      "searchboxDefaultIcon",
      lens::features::GetVisualSelectionUpdatesEnableGradientSuperG()
          ? "//resources/cr_components/searchbox/icons/google_g_gradient.svg"
          : "//resources/cr_components/searchbox/icons/google_g_cr23.svg");
  html_source->AddBoolean(
      "enableCsbMotionTweaks",
      lens::features::GetVisualSelectionUpdatesEnableCsbMotionTweaks());
  html_source->AddBoolean(
      "enableVisualSelectionUpdates",
      lens::features::IsLensOverlayVisualSelectionUpdatesEnabled());
  html_source->AddBoolean("reportMetrics", false);
  html_source->AddLocalizedString("searchBoxHintDefault",
                                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL);
  html_source->AddLocalizedString(
      "searchBoxHintPdf", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL_PDF);
  html_source->AddBoolean("isLensSearchbox", true);
  html_source->AddBoolean(
      "forceHideEllipsis",
      lens::features::GetVisualSelectionUpdatesHideCsbEllipsis());
  html_source->AddBoolean(
    "enableThumbnailSizingTweaks",
    lens::features::GetVisualSelectionUpdatesEnableThumbnailSizingTweaks());
  html_source->AddBoolean("steadyComposeboxShowVoiceSearch", false);
  html_source->AddBoolean("expandedComposeboxShowVoiceSearch", false);
  html_source->AddBoolean("expandedSearchboxShowVoiceSearch", false);
  html_source->AddBoolean("composeboxContextDragAndDropEnabled", false);

  // Determine if the cursor tooltip should appear.
  Profile* profile = Profile::FromWebUI(web_ui);
  int lens_overlay_start_count =
      profile->GetPrefs()->GetInteger(::prefs::kLensOverlayStartCount);
  html_source->AddBoolean(
      "canShowTooltipFromPrefs",
      lens_overlay_start_count <= kNumTimesToShowCursorTooltips);

  html_source->AddBoolean(
      "enablePrivacyNotice",
      lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled() &&
          !DidUserGrantLensOverlayNeededPermissions(profile->GetPrefs()));
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensPageHandlerFactory> receiver) {
  lens_page_factory_receiver_.reset();
  lens_page_factory_receiver_.Bind(std::move(receiver));
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensGhostLoaderPageHandlerFactory>
        receiver) {
  lens_ghost_loader_page_factory_receiver_.reset();
  lens_ghost_loader_page_factory_receiver_.Bind(std::move(receiver));
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver) {
  LensSearchboxController* controller =
      GetLensSearchController().lens_searchbox_controller();

  auto handler = std::make_unique<LensSearchboxHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents(), /*lens_searchbox_client=*/controller);
  controller->SetContextualSearchboxHandler(std::move(handler));
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

LensSearchController& LensOverlayUntrustedUI::GetLensSearchController() {
  LensSearchController* controller =
      LensSearchController::FromWebUIWebContents(web_ui()->GetWebContents());
  CHECK(controller);
  return *controller;
}

LensOverlayController& LensOverlayUntrustedUI::GetLensOverlayController() {
  LensOverlayController* controller =
      LensOverlayController::FromWebUIWebContents(web_ui()->GetWebContents());
  CHECK(controller);
  return *controller;
}

void LensOverlayUntrustedUI::CreatePageHandler(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  LensOverlayController& controller = GetLensOverlayController();

  controller.SetInvocationTimeForWebUIBinding(base::TimeTicks::Now());
  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlayController.
  controller.BindOverlay(std::move(receiver), std::move(page));
}

void LensOverlayUntrustedUI::CreateGhostLoaderPage(
    mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page) {
  LensSearchboxController* controller =
      GetLensSearchController().lens_searchbox_controller();

  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlayController.
  controller->BindOverlayGhostLoader(std::move(page));
}

void LensOverlayUntrustedUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{kLensOverlayTranslateButtonElementId});
}

LensOverlayUntrustedUI::~LensOverlayUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(LensOverlayUntrustedUI)

}  // namespace lens
