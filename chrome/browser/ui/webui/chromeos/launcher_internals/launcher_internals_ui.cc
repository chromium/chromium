// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/launcher_internals/launcher_internals_ui.h"

#include "base/containers/span.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/launcher_internals_resources.h"
#include "chrome/grit/launcher_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

LauncherInternalsUI::LauncherInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(chrome::kChromeUILauncherInternalsHost));
  webui::SetupWebUIDataSource(source.get(),
                              base::make_span(kLauncherInternalsResources,
                                              kLauncherInternalsResourcesSize),
                              IDR_LAUNCHER_INTERNALS_INDEX_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

LauncherInternalsUI::~LauncherInternalsUI() = default;

void LauncherInternalsUI::BindInterface(
    mojo::PendingReceiver<launcher_internals::mojom::PageHandlerFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void LauncherInternalsUI::CreatePageHandler(
    mojo::PendingRemote<launcher_internals::mojom::Page> page) {
  auto* search_controller =
      AppListClientImpl::GetInstance()->search_controller();
  if (!search_controller)
    return;

  page_handler_ = std::make_unique<LauncherInternalsHandler>(search_controller,
                                                             std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(LauncherInternalsUI)

}  // namespace chromeos
