// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/critical_actions/critical_actions_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/critical_actions/critical_actions_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/critical_actions_resources.h"
#include "chrome/grit/critical_actions_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace critical_actions {

CriticalActionsInternalsUIConfig::CriticalActionsInternalsUIConfig()
    : DefaultInternalWebUIConfig(
          chrome::kChromeUICriticalActionsInternalsHost) {}

CriticalActionsInternalsUIConfig::~CriticalActionsInternalsUIConfig() = default;

bool CriticalActionsInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context) != nullptr;
}

CriticalActionsUI::CriticalActionsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICriticalActionsInternalsHost);
  webui::SetupWebUIDataSource(source, kCriticalActionsResources,
                              IDR_CRITICAL_ACTIONS_INDEX_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(CriticalActionsUI)

CriticalActionsUI::~CriticalActionsUI() = default;

void CriticalActionsUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void CriticalActionsUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<CriticalActionsPageHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()));
}

}  // namespace critical_actions
