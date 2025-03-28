// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/signin_history_sync_optin_resources.h"
#include "chrome/grit/signin_history_sync_optin_resources_map.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

bool HistorySyncOptinUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(switches::kEnableHistorySyncOptin);
}

HistorySyncOptinUI::HistorySyncOptinUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  // Set up the chrome://history-sync-optin source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIHistorySyncOptinHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::span(kSigninHistorySyncOptinResources),
      IDR_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HTML);
}

HistorySyncOptinUI::~HistorySyncOptinUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistorySyncOptinUI)

void HistorySyncOptinUI::BindInterface(
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void HistorySyncOptinUI::CreateHistorySyncOptinHandler(
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<HistorySyncOptinHandler>(std::move(receiver),
                                                            std::move(page));
}
