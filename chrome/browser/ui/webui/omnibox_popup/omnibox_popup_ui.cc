// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include <atomic>
#include <string_view>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "chrome/grit/omnibox_popup_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

bool OmniboxPopupUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup);
}

OmniboxPopupUI::OmniboxPopupUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIOmniboxPopupHost);

  RealboxHandler::SetupWebUIDataSource(source, Profile::FromWebUI(web_ui));

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
    content::RenderFrameHost* host,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  // Extract SessionID from URL to select the omnibox that initiated page load.
  const GURL& url = host->GetWebUI()->GetWebContents()->GetLastCommittedURL();
  SessionID id = SessionID::InvalidValue();
  if (url.is_valid() && url.has_query()) {
    std::string_view spec(url.query_piece());
    url::Component query, key, value;
    query.len = static_cast<int>(spec.size());
    while (url::ExtractQueryKeyValue(spec, &query, &key, &value)) {
      if (key.is_nonempty() && value.is_nonempty()) {
        const std::string_view key_piece = spec.substr(key.begin, key.len);
        constexpr char kSessionIdKey[] = "session_id";
        if (key_piece == kSessionIdKey) {
          const std::string_view value_piece =
              spec.substr(value.begin, value.len);
          int value_int = 0;
          if (base::StringToInt(value_piece, &value_int)) {
            id = SessionID::FromSerializedValue(value_int);
          }
          break;
        }
      }
    }
  }

  if (id.is_valid()) {
    if (Browser* browser = chrome::FindBrowserWithID(id)) {
      OmniboxController* controller =
          browser->window()->GetLocationBar()->GetOmniboxView()->controller();

      handler_ = std::make_unique<RealboxHandler>(
          std::move(pending_page_handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents(), &metrics_reporter_,
          /*lens_searchbox_client=*/nullptr, controller);
    }
  }
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
