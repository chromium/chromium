// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"
#include "chrome/common/pref_names.h"
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
  html_source->AddLocalizedString("feedbackToastMessage",
                                  IDS_LENS_OVERLAY_FEEDBACK_TOAST_MESSAGE);
  html_source->AddLocalizedString("sendFeedbackButtonText",
                                  IDS_LENS_OVERLAY_SEND_FEEDBACK_BUTTON_LABEL);
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
  html_source->AddBoolean(
      "scrollToEnabled",
      lens::features::IsLensSearchSidePanelScrollToAPIEnabled());
  html_source->AddString("resultsSearchURL",
                         lens::features::GetLensOverlayResultsSearchURL());
  html_source->AddBoolean(
      "enableCloseButtonTweaks",
      lens::features::GetVisualSelectionUpdatesEnableCloseButtonTweaks());
  html_source->AddBoolean(
      "enableSummarizeSuggestionHint",
      lens::features::ShouldEnableSummarizeHintForContextualSuggest());

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
  html_source->AddLocalizedString("searchBoxHint",
                                  IDS_GOOGLE_LENS_SEARCH_BOX_EMPTY_HINT);
  html_source->AddLocalizedString("searchBoxHintMultimodal",
                                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MULTIMODAL);
  html_source->AddBoolean("isLensSearchbox", true);
  html_source->AddBoolean(
      "forceHideEllipsis",
      lens::features::GetVisualSelectionUpdatesHideCsbEllipsis());
  html_source->AddBoolean("queryAutocompleteOnEmptyInput", true);
  html_source->AddBoolean(
      "enableCsbMotionTweaks",
      lens::features::GetVisualSelectionUpdatesEnableCsbMotionTweaks());
  html_source->AddBoolean(
      "enableThumbnailSizingTweaks",
      lens::features::GetVisualSelectionUpdatesEnableThumbnailSizingTweaks());
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
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver) {
  LensSearchboxController* controller =
      GetLensSearchController().lens_searchbox_controller();

  auto handler = std::make_unique<LensSearchboxHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents(),
      /*metrics_reporter=*/nullptr, /*lens_searchbox_client=*/controller);
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
