// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_read_anything_resources.h"
#include "chrome/grit/side_panel_read_anything_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

ReadAnythingUIUntrustedConfig::ReadAnythingUIUntrustedConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIUntrustedReadAnythingSidePanelHost) {}

ReadAnythingUIUntrustedConfig::~ReadAnythingUIUntrustedConfig() = default;

std::unique_ptr<content::WebUIController>
ReadAnythingUIUntrustedConfig::CreateWebUIController(content::WebUI* web_ui,
                                                     const GURL& url) {
  return std::make_unique<ReadAnythingUI>(web_ui);
}

bool ReadAnythingUIUntrustedConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsReadAnythingEnabled();
}

ReadAnythingUI::ReadAnythingUI(content::WebUI* web_ui)
    : ui::UntrustedBubbleWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedReadAnythingSidePanelURL);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"readAnythingTabTitle", IDS_READING_MODE_TITLE},
      {"notSelectableHeader", IDS_READING_MODE_NOT_SELECTABLE_HEADER},
      {"emptyStateHeader", IDS_READING_MODE_EMPTY_STATE_HEADER},
      {"emptyStateSubheader", IDS_READING_MODE_EMPTY_STATE_SUBHEADER},
      {"readAnythingLoadingMessage", IDS_READ_ANYTHING_LOADING},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  // Rather than call `webui::SetupWebUIDataSource`, manually set up source
  // here. This ensures that if CSPs change in a way that is safe for chrome://
  // but not chrome-untrusted://, ReadAnythingUI does not inherit them.
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  webui::EnableTrustedTypesCSP(source);
  source->AddResourcePaths(base::make_span(
      kSidePanelReadAnythingResources, kSidePanelReadAnythingResourcesSize));
  source->AddResourcePath("", IDR_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' chrome-untrusted://resources;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome-untrusted://resources "
      "https://fonts.googleapis.com 'unsafe-inline';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc,
      "font-src 'self' chrome-untrusted://resources "
      "https://fonts.gstatic.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' chrome-untrusted://resources;");
}

ReadAnythingUI::~ReadAnythingUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadAnythingUI)

void ReadAnythingUI::BindInterface(
    mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandlerFactory>
        receiver) {
  read_anything_page_factory_receiver_.reset();
  read_anything_page_factory_receiver_.Bind(std::move(receiver));
}

void ReadAnythingUI::CreateUntrustedPageHandler(
    mojo::PendingRemote<read_anything::mojom::UntrustedPage> page,
    mojo::PendingReceiver<read_anything::mojom::UntrustedPageHandler>
        receiver) {
  DCHECK(page);
  read_anything_page_handler_ = std::make_unique<ReadAnythingPageHandler>(
      std::move(page), std::move(receiver), web_ui());
  if (embedder())
    embedder()->ShowUI();
}
