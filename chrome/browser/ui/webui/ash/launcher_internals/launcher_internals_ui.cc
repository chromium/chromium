// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals_ui.h"

#include "base/containers/span.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/launcher_internals_resources.h"
#include "chrome/grit/launcher_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

LauncherInternalsUI::LauncherInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUILauncherInternalsHost);
  webui::SetupWebUIDataSource(source,
                              base::make_span(kLauncherInternalsResources,
                                              kLauncherInternalsResourcesSize),
                              IDR_LAUNCHER_INTERNALS_INDEX_HTML);
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

}  // namespace ash
