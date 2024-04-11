// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_untrusted_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

const char kScreenshotPath[] = "screenshot.jpeg";

bool ShouldLoadScreenshot(const std::string& path) {
  return path == kScreenshotPath;
}

namespace lens {

LensUntrustedUI::LensUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  // This code path is invoked for both the overlay WebUI and the sidepanel
  // WebUI.

  // Set up the chrome-untrusted://lens source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUILensUntrustedURL);
  html_source->AddLocalizedString("close", IDS_CLOSE);

  // Allow FrameSrc from all Google subdomains as redirects can occur.
  GURL results_side_panel_url =
      GURL(lens::features::GetLensOverlayResultsSearchURL());
  std::string frame_src_directive =
      base::StrCat({"frame-src https://*.google.com ",
                    results_side_panel_url.GetWithEmptyPath().spec(), ";"});
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, frame_src_directive);

  // Allow ImgSrc and StyleSrc from chrome-untrusted:// paths for searchbox use.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' chrome-untrusted://resources;");
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome-untrusted://resources chrome-untrusted://theme");

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kLensUntrustedResources, kLensUntrustedResourcesSize),
      IDR_LENS_UNTRUSTED_LENS_OVERLAY_HTML);

  // Add required resources for the searchbox.
  SearchboxHandler::SetupWebUIDataSource(html_source,
                                         Profile::FromWebUI(web_ui));
  html_source->AddBoolean("reportMetrics", false);

  // Set request filter for loading the screenshot on the page.
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldLoadScreenshot),
      base::BindRepeating(&LensUntrustedUI::LoadScreenshot,
                          weak_factory_.GetWeakPtr()));
}

void LensUntrustedUI::LoadScreenshot(
    const std::string& resource_path,
    content::WebUIDataSource::GotDataCallback got_data_callback) {
  // Get the viewport screenshot
  const SkBitmap& screenshot_bitmap =
      LensOverlayController::GetController(web_ui())->current_screenshot();

  // Convert Bitmap into JPEG so it can easily be rendered in the WebUI
  // TODO(b/328294622): Increase quality if pixelated once rendered.
  // TODO(b/328630043): Ensure doing JPEG encoding on main thread does not cause
  // performance issues.
  std::vector<unsigned char> screenshot_image;
  if (!gfx::JPEGCodec::Encode(screenshot_bitmap, /*quality=*/90,
                              &screenshot_image)) {
    // If encoding fails, the output to the vector is unknown, so we clear to
    // make sure there is not bad data.
    std::move(got_data_callback).Run(nullptr);
    return;
  }

  std::move(got_data_callback)
      .Run(base::RefCountedBytes::TakeVector(&screenshot_image));
}

void LensUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensPageHandlerFactory> receiver) {
  lens_page_factory_receiver_.reset();
  lens_page_factory_receiver_.Bind(std::move(receiver));
}

void LensUntrustedUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver) {
  LensOverlayController* controller =
      LensOverlayController::GetController(web_ui());
  auto handler = std::make_unique<RealboxHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents(),
      /*metrics_reporter=*/nullptr, /*lens_searchbox_client=*/controller,
      /*omnibox_controller=*/nullptr);
  controller->SetSearchboxHandler(std::move(handler));
}

void LensUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void LensUntrustedUI::CreatePageHandler(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlayController.
  LensOverlayController::GetController(web_ui())->BindOverlay(
      std::move(receiver), std::move(page));
}

void LensUntrustedUI::CreateSidePanelPageHandler(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensSidePanelPage> page) {
  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlayController.
  LensOverlayController::GetController(web_ui())->BindSidePanel(
      std::move(receiver), std::move(page));
}

LensUntrustedUI::~LensUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(LensUntrustedUI)

}  // namespace lens
