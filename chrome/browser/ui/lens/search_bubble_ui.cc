// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/search_bubble_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/search_bubble_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/lens_search_bubble_resources.h"
#include "chrome/grit/lens_search_bubble_resources_map.h"
#include "components/lens/lens_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

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
}

SearchBubbleUI::~SearchBubbleUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(SearchBubbleUI)

void SearchBubbleUI::BindInterface(
    mojo::PendingReceiver<lens::mojom::SearchBubblePageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
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
