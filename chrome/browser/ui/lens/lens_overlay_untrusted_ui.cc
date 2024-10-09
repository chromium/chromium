// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/lens_shared_resources.h"
#include "chrome/grit/lens_shared_resources_map.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace lens {

// The number of times to show cursor tooltips.
constexpr int kNumTimesToShowCursorTooltips = 5;

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
  html_source->AddLocalizedString("selectText", IDS_LENS_OVERLAY_SELECT_TEXT);
  html_source->AddLocalizedString(
      "networkErrorPageTopLine",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_FIRST_LINE);
  html_source->AddLocalizedString(
      "networkErrorPageBottomLine",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_SECOND_LINE);
  html_source->AddLocalizedString("detectLanguage",
                                  IDS_LENS_OVERLAY_DETECT_LANGUAGE_LABEL);
  html_source->AddLocalizedString(
      "translateFrom", IDS_LENS_OVERLAY_SOURCE_LANGUAGE_PICKER_MENU_TITLE);
  html_source->AddLocalizedString(
      "translateTo", IDS_LENS_OVERLAY_TARGET_LANGUAGE_PICKER_MENU_TITLE);
  html_source->AddLocalizedString("allLanguages",
                                  IDS_LENS_OVERLAY_ALL_LANGUAGES_LABEL);
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
  html_source->AddBoolean("useShimmerCanvas",
                          lens::features::GetLensOverlayUseShimmerCanvas());
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
  html_source->AddInteger(
      "textReceivedTimeout",
      lens::features::
          GetLensOverlayImageContextMenuActionsTextReceivedTimeout());
  html_source->AddBoolean(
      "darkMode",
      lens::LensOverlayShouldUseDarkMode(
          ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui))));
  html_source->AddBoolean(
      "enableOverlayContextualSearchbox",
      lens::features::IsLensOverlayContextualSearchboxEnabled());

  // Controller doesn't exist in unsupported context but WebUI should still
  // load.
  if (auto* controller =
          LensOverlayController::GetControllerFromWebViewWebContents(
              web_ui->GetWebContents())) {
    html_source->AddDouble("invocationTime",
                           controller->GetInvocationTimeSinceEpoch());
    html_source->AddString("invocationSource",
                           controller->GetInvocationSourceString());
  }

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
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kLensUntrustedResources, kLensUntrustedResourcesSize),
      IDR_LENS_UNTRUSTED_LENS_OVERLAY_HTML);

  html_source->AddResourcePaths(
      base::make_span(kLensSharedResources, kLensSharedResourcesSize));

  // Add required resources for the searchbox.
  SearchboxHandler::SetupWebUIDataSource(html_source,
                                         Profile::FromWebUI(web_ui));
  html_source->AddString(
      "searchboxDefaultIcon",
      "//resources/cr_components/searchbox/icons/google_g_cr23.svg");
  html_source->AddBoolean("reportMetrics", false);
  html_source->AddLocalizedString("searchBoxHint",
                                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL);
  html_source->AddBoolean("isLensSearchbox", true);

  // Determine if the cursor tooltip should appear.
  Profile* profile = Profile::FromWebUI(web_ui);
  int lens_overlay_start_count =
      profile->GetPrefs()->GetInteger(prefs::kLensOverlayStartCount);
  html_source->AddBoolean(
      "canShowTooltipFromPrefs",
      lens_overlay_start_count <= kNumTimesToShowCursorTooltips);
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensPageHandlerFactory> receiver) {
  lens_page_factory_receiver_.reset();
  lens_page_factory_receiver_.Bind(std::move(receiver));
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver) {
  LensOverlayController* controller =
      LensOverlayController::GetController(web_ui());
  // TODO(crbug.com/360724768): This should not need to be null-checked and
  // exists here as a temporary solution to handle situations where lens may be
  // loaded in an unsupported context (e.g. browser tab). Remove this once work
  // to restrict WebUI loading to relevant contexts has landed.
  if (!controller) {
    return;
  }
  auto handler = std::make_unique<RealboxHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents(),
      /*metrics_reporter=*/nullptr, /*lens_searchbox_client=*/controller,
      /*omnibox_controller=*/nullptr);
  controller->SetContextualSearchboxHandler(std::move(handler));
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void LensOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void LensOverlayUntrustedUI::CreatePageHandler(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  LensOverlayController* controller =
      LensOverlayController::GetController(web_ui());
  // TODO(crbug.com/360724768): This should not need to be null-checked and
  // exists here as a temporary solution to handle situations where lens may be
  // loaded in an unsupported context (e.g. browser tab). Remove this once work
  // to restrict WebUI loading to relevant contexts has landed.
  if (!controller) {
    return;
  }
  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlayController.
  controller->BindOverlay(std::move(receiver), std::move(page));
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
