// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/download_shelf_resources.h"
#include "chrome/grit/download_shelf_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

DownloadShelfUI::DownloadShelfUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDownloadShelfHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kDownloadShelfResources, kDownloadShelfResourcesSize),
      IDR_DOWNLOAD_SHELF_DOWNLOAD_SHELF_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

DownloadShelfUI::~DownloadShelfUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(DownloadShelfUI)

void DownloadShelfUI::BindInterface(
    mojo::PendingReceiver<download_shelf::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void DownloadShelfUI::CreatePageHandler(
    mojo::PendingRemote<download_shelf::mojom::Page> page,
    mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<DownloadShelfPageHandler>(
      std::move(receiver), std::move(page), web_ui(), this);
}
