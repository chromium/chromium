// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_untrusted_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

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
