// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/reload_button_resources.h"
#include "chrome/grit/reload_button_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

ReloadButtonUI::ReloadButtonUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIReloadButtonHost);

  webui::SetupWebUIDataSource(source, kReloadButtonResources,
                              IDR_RELOAD_BUTTON_RELOAD_BUTTON_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ReloadButtonUI)

ReloadButtonUI::~ReloadButtonUI() = default;

ReloadButtonUIConfig::ReloadButtonUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIReloadButtonHost) {}

bool ReloadButtonUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsWebUIReloadButtonEnabled();
}

void ReloadButtonUI::BindInterface(
    mojo::PendingReceiver<reload_button::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ReloadButtonUI::SetLoadingState(bool is_loading, bool force) {
  if (page_handler_) {
    page_handler_->SetLoadingState(is_loading, force);
  }
}

void ReloadButtonUI::CreatePageHandler(
    mojo::PendingRemote<reload_button::mojom::Page> page,
    mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver) {
  CHECK(page);
  page_handler_ = std::make_unique<ReloadButtonPageHandler>(
      std::move(receiver), std::move(page), web_ui()->GetWebContents());
}
