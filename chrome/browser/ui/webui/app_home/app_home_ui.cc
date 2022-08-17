// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/app_home_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_data_source.h"

namespace webapps {

AppHomeUI::AppHomeUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAppLauncherPageHost);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

void AppHomeUI::BindInterface(
    mojo::PendingReceiver<app_home::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();

  page_factory_receiver_.Bind(std::move(receiver));
}

void AppHomeUI::CreatePageHandler(
    mojo::PendingRemote<app_home::mojom::Page> page,
    mojo::PendingReceiver<app_home::mojom::PageHandler> receiver) {
  DCHECK(page);
  Profile* profile = Profile::FromWebUI(web_ui());
  page_handler_ = std::make_unique<AppHomePageHandler>(
      web_ui(), profile, std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(AppHomeUI)

AppHomeUI::~AppHomeUI() = default;

}  // namespace webapps
