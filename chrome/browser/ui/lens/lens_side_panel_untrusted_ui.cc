// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
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

LensSidePanelUntrustedUI::LensSidePanelUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  // Set up the chrome-untrusted://lens/ source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUILensUntrustedSidePanelURL);
  html_source->AddLocalizedString("backButton", IDS_ACCNAME_BACK);
  html_source->AddLocalizedString(
      "networkErrorPageTopLine",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_FIRST_LINE);
  html_source->AddLocalizedString(
      "networkErrorPageBottomLine",
      IDS_SIDE_PANEL_COMPANION_ERROR_PAGE_SECOND_LINE);

  // Add finch flags
  html_source->AddString(
      "resultsLoadingUrl",
      lens::features::GetLensOverlayResultsSearchLoadingURL(
          lens::LensOverlayShouldUseDarkMode(
              ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui)))));
  html_source->AddBoolean(
      "darkMode",
      lens::LensOverlayShouldUseDarkMode(
          ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui))));
  html_source->AddBoolean("enableErrorPage",
                          lens::features::GetLensOverlayEnableErrorPage());

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
      IDR_LENS_UNTRUSTED_SIDE_PANEL_SIDE_PANEL_HTML);

  html_source->AddResourcePaths(
      base::make_span(kLensSharedResources, kLensSharedResourcesSize));

  // Add required resources for the searchbox.
  SearchboxHandler::SetupWebUIDataSource(html_source,
                                         Profile::FromWebUI(web_ui));
  html_source->AddString(
      "searchboxDefaultIcon",
      "//resources/cr_components/searchbox/icons/google_g.svg");
  html_source->AddBoolean("reportMetrics", false);
  html_source->AddLocalizedString("searchBoxHint",
                                  IDS_GOOGLE_LENS_SEARCH_BOX_EMPTY_HINT);
  html_source->AddLocalizedString("searchBoxHintMultimodal",
                                  IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MULTIMODAL);
  html_source->AddBoolean("isLensSearchbox", true);
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandlerFactory>
        receiver) {
  lens_side_panel_page_factory_receiver_.reset();
  lens_side_panel_page_factory_receiver_.Bind(std::move(receiver));
}

void LensSidePanelUntrustedUI::BindInterface(
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
  controller->SetSidePanelSearchboxHandler(std::move(handler));
}

void LensSidePanelUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void LensSidePanelUntrustedUI::CreateSidePanelPageHandler(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensSidePanelPage> page) {
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
  controller->BindSidePanel(std::move(receiver), std::move(page));
}

LensSidePanelUntrustedUI::~LensSidePanelUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(LensSidePanelUntrustedUI)

}  // namespace lens
