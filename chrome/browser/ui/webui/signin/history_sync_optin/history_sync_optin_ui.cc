// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
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
    : ui::MojoWebUIController(web_ui, true),
      profile_(Profile::FromWebUI(web_ui)) {
  // Set up the chrome://history-sync-optin source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIHistorySyncOptinHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::span(kSigninHistorySyncOptinResources),
      IDR_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"historySyncOptInTitle", IDS_HISTORY_SYNC_OPT_IN_TITLE},
      {"historySyncOptInSubtitle", IDS_HISTORY_SYNC_OPT_IN_SUBTITLE},
      {"historySyncOptInAcceptButtonLabel",
       IDS_HISTORY_SYNC_OPT_IN_ACCEPT_BUTTON},
      {"historySyncOptInRejectButtonLabel",
       IDS_HISTORY_SYNC_OPT_IN_REJECT_BUTTON},
      {"historySyncOptInDescription", IDS_HISTORY_SYNC_OPT_IN_DESCRIPTION},
  };

  source->AddLocalizedStrings(kLocalizedStrings);
  // Add avatar fallback value.
  source->AddString("accountPictureUrl",
                    profiles::GetPlaceholderAvatarIconUrl());
}

HistorySyncOptinUI::~HistorySyncOptinUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(HistorySyncOptinUI)

void HistorySyncOptinUI::BindInterface(
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void HistorySyncOptinUI::Initialize(Browser* browser) {
  initialize_handler_callback_ =
      base::BindOnce(&HistorySyncOptinUI::OnMojoHandlersReady,
                     weak_ptr_factory_.GetWeakPtr(), browser);
}

void HistorySyncOptinUI::CreateHistorySyncOptinHandler(
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver) {
  CHECK(page);
  CHECK(receiver);
  CHECK(initialize_handler_callback_);
  std::move(initialize_handler_callback_)
      .Run(std::move(page), std::move(receiver));
}

void HistorySyncOptinUI::OnMojoHandlersReady(
    Browser* browser,
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver) {
  CHECK(!page_handler_);
  page_handler_ = std::make_unique<HistorySyncOptinHandler>(
      std::move(receiver), std::move(page), browser, profile_);
}
