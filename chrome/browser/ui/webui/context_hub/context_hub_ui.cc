// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_ui.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/context_hub_resources.h"
#include "chrome/grit/context_hub_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/favicon_source.h"  // nogncheck
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/url_data_source.h"
#endif

ContextHubUI::ContextHubUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIContextHubHost);

  webui::SetupWebUIDataSource(source, kContextHubResources,
                              IDR_CONTEXT_HUB_CONTEXT_HUB_HTML);

  source->AddResourcePath("save_to_memory_bank",
                          IDR_CONTEXT_HUB_SAVE_TO_MEMORY_BANK_HTML);

  source->AddBoolean("kAutoTabGroups",
                     base::FeatureList::IsEnabled(
                         browser::context_hub::mojom::kAutoTabGroups));
  source->AddBoolean(
      "kAutoTodos",
      base::FeatureList::IsEnabled(browser::context_hub::mojom::kAutoTodos));
  source->AddInteger("kMaxTabGroupChatHistoryTurns",
                     context_hub::features::kMaxTabGroupChatHistoryTurns.Get());
  source->AddInteger(
      "kMaxMemoryBankChatHistoryTurns",
      context_hub::features::kMaxMemoryBankChatHistoryTurns.Get());

#if !BUILDFLAG(IS_ANDROID)
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
#endif
}

ContextHubUI::~ContextHubUI() = default;

void ContextHubUI::BindInterface(
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ContextHubUI::CreatePageHandler(
    mojo::PendingRemote<browser::context_hub::mojom::Page> page,
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> handler) {
  page_handler_ = std::make_unique<ContextHubPageHandler>(
      std::move(page), std::move(handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextHubUI)
