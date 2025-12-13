// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_composebox_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/lens_shared_resources.h"
#include "chrome/grit/lens_shared_resources_map.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "ui/webui/webui_util.h"

namespace lens {

LensSidePanelUntrustedUI::LensSidePanelUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  // Set up the chrome-untrusted://lens/ source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUILensUntrustedSidePanelURL);
  html_source->AddLocalizedString("backButton", IDS_ACCNAME_BACK);
  html_source->AddLocalizedString("close", IDS_CLOSE);
  html_source->AddLocalizedString("dismiss",
                                  IDS_LENS_OVERLAY_TOAST_DISMISS_MESSAGE);
  html_source->AddLocalizedString(
      "networkErrorPageTopLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_GENERIC_ERROR_PAGE_FIRST_LINE);
  html_source->AddLocalizedString(
      "networkErrorPageBottomLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_GENERIC_ERROR_PAGE_SECOND_LINE);
  html_source->AddLocalizedString(
      "protectedErrorPageTopLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_FIRST_LINE);
  html_source->AddLocalizedString(
      "protectedErrorPageBottomLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_SECOND_LINE);
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
      "feedbackToastMessage", lens::features::IsLensUpdatedFeedbackEnabled()
                                  ? IDS_LENS_OVERLAY_FEEDBACK_TOAST_MESSAGE_ALT
                                  : IDS_LENS_OVERLAY_FEEDBACK_TOAST_MESSAGE);
  html_source->AddLocalizedString("sendFeedbackButtonText",
                                  IDS_LENS_OVERLAY_SEND_FEEDBACK_BUTTON_LABEL);
  html_source->AddLocalizedString(
      "closeFeedbackToastAccessibilityLabel",
      IDS_LENS_OVERLAY_CLOSE_FEEDBACK_TOAST_ACCESSIBILITY_LABEL);
  const bool dark_mode = lens::LensOverlayShouldUseDarkMode(
      ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui)));

  // Add finch flags
  html_source->AddString(
      "resultsLoadingUrl",
      lens::features::GetLensOverlayResultsSearchLoadingURL(dark_mode));
  html_source->AddBoolean("darkMode", dark_mode);
  html_source->AddBoolean("enableErrorPage",
                          lens::features::GetLensOverlayEnableErrorPage());
  html_source->AddBoolean(
      "enableGhostLoader",
      lens::features::EnableContextualSearchboxGhostLoader());
  html_source->AddBoolean(
      "showContextualSearchboxLoadingState",
      lens::features::ShowContextualSearchboxGhostLoaderLoadingState());
  html_source->AddLocalizedString("searchBoxHintContextualDefault",
                                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL);
  html_source->AddLocalizedString(
      "searchBoxHintContextualPdf",
      IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_CONTEXTUAL_PDF);
  html_source->AddBoolean(
      "newFeedbackEnabled",
      lens::features::IsLensSearchSidePanelNewFeedbackEnabled());
  html_source->AddInteger(
      "updatedFeedbackToastTimeoutMs",
      lens::features::GetLensUpdatedFeedbackToastTimeoutMs());
  html_source->AddString("resultsSearchURL",
                         lens::features::GetLensOverlayResultsSearchURL());
  html_source->AddBoolean(
      "enableCloseButtonTweaks",
      lens::features::GetVisualSelectionUpdatesEnableCloseButtonTweaks());
  html_source->AddBoolean(
      "enableSummarizeSuggestionHint",
      lens::features::ShouldEnableSummarizeHintForContextualSuggest());
  html_source->AddBoolean(
      "enableWebviewResults",
      lens::features::IsLensSidePanelWebviewResultsEnabled());
  html_source->AddBoolean("enableLensAimSuggestions",
                          lens::features::GetAimSuggestionsEnabled());
  html_source->AddBoolean(
      "enableLensAimSuggestionsGradientBackground",
      lens::features::GetAimSuggestionsGradientBackgroundEnabled());

  // Aim M3 flags
  const bool aim_enabled = lens::IsAimM3Enabled(Profile::FromWebUI(web_ui));
  html_source->AddBoolean(
      "enableFloatingGForHeader",
      aim_enabled && lens::features::GetEnableFloatingGForHeader());
  html_source->AddBoolean(
      "enableClientSideAimHeader",
      aim_enabled && lens::features::GetEnableClientSideHeader());
  html_source->AddBoolean(
      "enableAimSearchbox",
      aim_enabled && lens::features::GetAimSearchboxEnabled());
  html_source->AddBoolean("showLensButton",
                          lens::features::GetEnableLensButtonInSearchbox());
  html_source->AddBoolean("updatedFeedbackEnabled",
                          aim_enabled &&
                              lens::features::GetAimSearchboxEnabled() &&
                              lens::features::IsLensUpdatedFeedbackEnabled());

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

  // Support no file types.
  html_source->AddString("composeboxImageFileTypes", "");
  html_source->AddString("composeboxAttachmentFileTypes", "");
  html_source->AddInteger("composeboxFileMaxSize", 0);
  html_source->AddInteger("composeboxFileMaxCount", 0);
  // Typed suggest is only enabled for typeahead suggestions.
  html_source->AddBoolean(
      "composeboxShowTypedSuggest",
      lens::features::IsLensAimTypeAheadSuggestionsEnabled());
  html_source->AddBoolean("composeboxShowTypedSuggestWithContext", true);
  // Enable ZPS if suggestions are enabled.
  html_source->AddBoolean("composeboxShowZps",
                          lens::features::GetAimSuggestionsEnabled());
  // Disable image context suggestions.
  html_source->AddBoolean("composeboxShowImageSuggest", false);
  // Disable context menu and related features.
  html_source->AddBoolean("composeboxShowContextMenu", false);
  html_source->AddBoolean("composeboxShowContextMenuDescription", true);
  // Send event when escape is pressed.
  html_source->AddBoolean("composeboxCloseByEscape", true);
  html_source->AddBoolean("composeboxShowLensSearchChip", false);
  html_source->AddBoolean("composeboxShowRecentTabChip", false);
  // Enable submit button.
  html_source->AddBoolean("composeboxShowSubmit", true);
  // Enables a fix that causes no flickering when transitioning between ZPS and
  // typed suggestions.
  html_source->AddBoolean("composeboxNoFlickerSuggestionsFix", true);
  // Specify metrics source.
  html_source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kLens));

  // Add strings for post message communication with the remote UI.
  lens::ClientToAimMessage handshake_ping;
  handshake_ping.mutable_handshake_ping()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  html_source->AddString("handshakeMessage",
                         handshake_ping.SerializeAsString());

  // Add required resources.
  webui::SetupWebUIDataSource(html_source, kLensUntrustedResources,
                              IDR_LENS_UNTRUSTED_SIDE_PANEL_SIDE_PANEL_HTML);

  html_source->AddResourcePaths(kLensSharedResources);

  // Add required resources for the searchbox.
  SearchboxHandler::SetupWebUIDataSource(html_source,
                                         Profile::FromWebUI(web_ui));
  html_source->AddString(
      "searchboxDefaultIcon",
      lens::features::GetVisualSelectionUpdatesEnableGradientSuperG()
          ? "//resources/cr_components/searchbox/icons/google_g_gradient.svg"
      : dark_mode
          ? "//resources/cr_components/searchbox/icons/google_g_cr23.svg"
          : "//resources/cr_components/searchbox/icons/google_g.svg");
  html_source->AddBoolean("reportMetrics", false);
  html_source->AddLocalizedString(
      "lensSearchButtonLabel",
      IDS_TOOLTIP_LENS_REINVOKE_VISUAL_SELECTION_A11Y_LABEL);
  html_source->AddLocalizedString("searchBoxHint",
                                  IDS_GOOGLE_LENS_SEARCH_BOX_EMPTY_HINT);
  html_source->AddLocalizedString("searchBoxHintMultimodal",
                                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MULTIMODAL);
  html_source->AddBoolean("isLensSearchbox", true);
  html_source->AddBoolean(
      "forceHideEllipsis",
      lens::features::GetVisualSelectionUpdatesHideCsbEllipsis());
  html_source->AddBoolean(
      "enableCsbMotionTweaks",
      lens::features::GetVisualSelectionUpdatesEnableCsbMotionTweaks());
  html_source->AddBoolean(
      "enableVisualSelectionUpdates",
      lens::features::IsLensOverlayVisualSelectionUpdatesEnabled());
  html_source->AddBoolean(
      "enableThumbnailSizingTweaks",
      lens::features::GetVisualSelectionUpdatesEnableThumbnailSizingTweaks());
  html_source->AddString(
      "searchboxComposePlaceholder",
      l10n_util::GetStringUTF8(IDS_LENS_COMPOSEBOX_HINT_TEXT));
  html_source->AddBoolean("composeboxShowPdfUpload", false);
  html_source->AddBoolean("composeboxSmartComposeEnabled", false);
  html_source->AddBoolean("composeboxShowDeepSearchButton", false);
  html_source->AddBoolean("composeboxShowCreateImageButton", false);
  html_source->AddBoolean("composeboxContextDragAndDropEnabled", false);
  html_source->AddBoolean("steadyComposeboxShowVoiceSearch", false);
  html_source->AddBoolean("expandedComposeboxShowVoiceSearch", false);
  html_source->AddBoolean("expandedSearchboxShowVoiceSearch", false);

  // If the ThemeSource isn't added here, since this WebUI is
  // chrome-untrusted, it will be unable to load stylesheets until a new tab
  // is opened.
  content::URLDataSource::Add(
      Profile::FromWebUI(web_ui),
      std::make_unique<ThemeSource>(Profile::FromWebUI(web_ui),
                                    /*serve_untrusted=*/true));
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandlerFactory>
        receiver) {
  lens_side_panel_page_factory_receiver_.reset();
  lens_side_panel_page_factory_receiver_.Bind(std::move(receiver));
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensGhostLoaderPageHandlerFactory>
        receiver) {
  lens_ghost_loader_page_factory_receiver_.reset();
  lens_ghost_loader_page_factory_receiver_.Bind(std::move(receiver));
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver) {
  LensSearchboxController* controller =
      GetLensSearchController().lens_searchbox_controller();

  auto handler = std::make_unique<LensSearchboxHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents(), /*lens_searchbox_client=*/controller);
  controller->SetSidePanelSearchboxHandler(std::move(handler));
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(receiver));
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  composebox_page_handler_factory_receiver_.reset();
  composebox_page_handler_factory_receiver_.Bind(std::move(receiver));
}

void LensSidePanelUntrustedUI::CreatePageHandler(
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  DCHECK(pending_page.is_valid());
  auto* controller = GetLensSearchController().lens_composebox_controller();
  controller->BindComposebox(
      std::move(pending_page_handler), std::move(pending_page),
      std::move(pending_searchbox_page), std::move(pending_searchbox_handler));
}

LensSearchController& LensSidePanelUntrustedUI::GetLensSearchController() {
  LensSearchController* controller =
      LensSearchController::FromWebUIWebContents(web_ui()->GetWebContents());
  CHECK(controller);
  return *controller;
}

void LensSidePanelUntrustedUI::CreateSidePanelPageHandler(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensSidePanelPage> page) {
  LensSearchController& controller = GetLensSearchController();

  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlaySidePanelCoordinator.
  controller.lens_overlay_side_panel_coordinator()->BindSidePanel(
      std::move(receiver), std::move(page));
}

void LensSidePanelUntrustedUI::CreateGhostLoaderPage(
    mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page) {
  LensSearchboxController* controller =
      GetLensSearchController().lens_searchbox_controller();

  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlayController.

  controller->BindSidePanelGhostLoader(std::move(page));
}

void LensSidePanelUntrustedUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{kLensSidePanelSearchBoxElementId});
}

LensSidePanelUntrustedUI::~LensSidePanelUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(LensSidePanelUntrustedUI)

}  // namespace lens
