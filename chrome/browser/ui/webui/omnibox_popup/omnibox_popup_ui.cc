// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include <atomic>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/realbox/realbox_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace {

// This exists to avoid a race condition in RealboxHandler. It's better to
// rely on a specific variable controlled by local omnibox logic than to
// get the last active browser which could be changed by anything at any time.
// This can be eliminated if we find a way to cleanly pass pointers into WebUI
// construction, but currently they appear designed to avoid such things,
// sensibly relying on the URL only.
std::atomic<OmniboxController*> g_omnibox_controller = nullptr;

}  // namespace

// static
void OmniboxPopupUI::SetOmniboxController(
    OmniboxController* omnibox_controller) {
  g_omnibox_controller = omnibox_controller;
}

OmniboxPopupUI::OmniboxPopupUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIOmniboxPopupHost);

  RealboxHandler::SetupDropdownWebUIDataSource(source,
                                               Profile::FromWebUI(web_ui));

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kOmniboxPopupResources, kOmniboxPopupResourcesSize),
      IDR_OMNIBOX_POPUP_OMNIBOX_POPUP_HTML);
  webui::EnableTrustedTypesCSP(source);

  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
}

OmniboxPopupUI::~OmniboxPopupUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxPopupUI)

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<omnibox::mojom::PageHandler> pending_page_handler) {
  OmniboxController* controller = g_omnibox_controller;
  g_omnibox_controller = nullptr;

  handler_ = std::make_unique<RealboxHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents(), &metrics_reporter_, controller);
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  metrics_reporter_.BindInterface(std::move(receiver));
}

void OmniboxPopupUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}
