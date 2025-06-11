// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/actor_internals/actor_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/actor_internals/actor_internals_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/actor_internals_resources.h"
#include "chrome/grit/actor_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

ActorInternalsUI::ActorInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  // Set up the chrome://actor-internals/ source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIActorInternalsHost);
  webui::SetupWebUIDataSource(source, kActorInternalsResources,
                              IDR_ACTOR_INTERNALS_ACTOR_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ActorInternalsUI)

ActorInternalsUI::~ActorInternalsUI() = default;

void ActorInternalsUI::BindInterface(
    mojo::PendingReceiver<actor_internals::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ActorInternalsUI::CreatePageHandler(
    mojo::PendingRemote<actor_internals::mojom::Page> page,
    mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<ActorInternalsUIHandler>(
      web_ui()->GetWebContents(), std::move(page), std::move(receiver));
}
