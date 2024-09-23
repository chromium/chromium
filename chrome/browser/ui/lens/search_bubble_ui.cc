// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/lens/search_bubble_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/search_bubble_page_handler.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/lens_search_bubble_resources.h"
#include "chrome/grit/lens_search_bubble_resources_map.h"
#include "chrome/grit/lens_shared_resources.h"
#include "chrome/grit/lens_shared_resources_map.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace lens {

SearchBubbleUI::SearchBubbleUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui), web_ui_(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui_), chrome::kChromeUILensSearchBubbleHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"close", IDS_CLOSE}};
  for (const auto& str : kLocalizedStrings) {
    webui::AddLocalizedString(source, str.name, str.id);
  }
  webui::SetupWebUIDataSource(source,
                              base::make_span(kLensSearchBubbleResources,
                                              kLensSearchBubbleResourcesSize),
                              IDR_LENS_SEARCH_BUBBLE_SEARCH_BUBBLE_HTML);
  // Add required resources for the searchbox.
  SearchboxHandler::SetupWebUIDataSource(source, Profile::FromWebUI(web_ui));
  source->AddBoolean("reportMetrics", false);
  source->AddString("searchboxDefaultIcon",
                    "//resources/cr_components/searchbox/icons/google_g.svg");
  source->AddLocalizedString("searchBoxHint",
                             IDS_GOOGLE_LENS_SEARCH_BOX_EMPTY_HINT);
  source->AddBoolean(
      "darkMode",
      lens::LensOverlayShouldUseDarkMode(
          ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui))));
  source->AddBoolean("isLensSearchbox", true);
  source->AddResourcePaths(
      base::make_span(kLensSharedResources, kLensSharedResourcesSize));
}

SearchBubbleUI::~SearchBubbleUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(SearchBubbleUI)

void SearchBubbleUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::SearchBubblePageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void SearchBubbleUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver) {
  LensOverlayController* controller =
      LensOverlayController::GetController(web_ui());
  CHECK(controller);
  auto contextual_searchbox_handler = std::make_unique<RealboxHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui_),
      web_ui()->GetWebContents(),
      /*metrics_reporter=*/nullptr, /*lens_searchbox_client=*/controller,
      /*omnibox_controller=*/nullptr);
  controller->SetContextualSearchboxHandler(
      std::move(contextual_searchbox_handler));
}

void SearchBubbleUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void SearchBubbleUI::CreatePageHandler(
    mojo::PendingRemote<lens::mojom::SearchBubblePage> page,
    mojo::PendingReceiver<lens::mojom::SearchBubblePageHandler> receiver) {
  page_handler_ = std::make_unique<SearchBubblePageHandler>(
      this, std::move(receiver), std::move(page), web_ui_->GetWebContents(),
      Profile::FromWebUI(web_ui_)->GetPrefs());
}

SearchBubbleUIConfig::SearchBubbleUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUILensSearchBubbleHost) {}

bool SearchBubbleUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return lens::features::IsLensOverlayEnabled();
}

bool SearchBubbleUIConfig::ShouldAutoResizeHost() {
  return true;
}

}  // namespace lens
