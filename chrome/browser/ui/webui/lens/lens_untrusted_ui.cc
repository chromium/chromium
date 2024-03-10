// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_untrusted_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/gfx/codec/jpeg_codec.h"

const char kScreenshotPath[] = "screenshot.jpeg";

bool ShouldLoadScreenshot(const std::string& path) {
  return path == kScreenshotPath;
}

namespace lens {

LensUntrustedUI::LensUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedBubbleWebUIController(web_ui) {
  // Set up the chrome-untrusted://lens source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUILensUntrustedURL);
  html_source->AddLocalizedString("close", IDS_CLOSE);

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kLensUntrustedResources, kLensUntrustedResourcesSize),
      IDR_LENS_UNTRUSTED_LENS_OVERLAY_HTML);

  // Get the viewport screenshot
  Browser* browser =
      chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui));
  CHECK(browser);

  const SkBitmap& screenshot_bitmap = browser->tab_strip_model()
                                          ->GetActiveTab()
                                          ->lens_overlay_controller()
                                          ->current_screenshot();

  // Convert Bitmap into JPEG so it can easily be rendered in the WebUI
  // TODO(b/328294622): Increase quality if pixelated once rendered.
  // TODO(b/328630043): Ensure doing JPEG encoding on main thread does not cause
  // performance issues.
  if (!gfx::JPEGCodec::Encode(screenshot_bitmap, /*quality=*/90,
                              &screenshot_image_)) {
    // If encoding fails, the output to the vector is unknown, so we clear to
    // make sure there is not bad data.
    screenshot_image_.clear();
    return;
  }

  // Set request filter for loading the screenshot on the page.
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldLoadScreenshot),
      base::BindRepeating(&LensUntrustedUI::LoadScreenshot,
                          weak_factory_.GetWeakPtr()));
}

void LensUntrustedUI::LoadScreenshot(
    const std::string& resource_path,
    content::WebUIDataSource::GotDataCallback got_data_callback) {
  if (!screenshot_image_.empty()) {
    std::move(got_data_callback)
        .Run(base::RefCountedBytes::TakeVector(&screenshot_image_));
  } else {
    std::move(got_data_callback).Run(nullptr);
  }
}

void LensUntrustedUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::LensPageHandlerFactory> receiver) {
  lens_page_factory_receiver_.reset();
  lens_page_factory_receiver_.Bind(std::move(receiver));
}

void LensUntrustedUI::CreatePageHandler(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  // Once the interface is bound, we want to connect this instance with the
  // appropriate instance of LensOverlayController.
  LensOverlayController::BindOverlay(web_ui(), std::move(receiver),
                                     std::move(page));
}

LensUntrustedUI::~LensUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(LensUntrustedUI)

}  // namespace lens
