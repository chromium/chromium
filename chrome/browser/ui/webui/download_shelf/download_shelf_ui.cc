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
    : ui::MojoWebUIController(web_ui, true),
      download_manager_(content::BrowserContext::GetDownloadManager(
          Profile::FromWebUI(web_ui))) {
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

void DownloadShelfUI::ShowContextMenu(uint32_t download_id,
                                      int32_t client_x,
                                      int32_t client_y) {
  DownloadUIModel* download_ui_model = FindDownloadById(download_id);
  if (!download_ui_model) {
    // TODO: Remove this block. After we implement
    // DownloadShelf::DoShowDownload(), FindDownloadById() should always find a
    // DownloadUIMdodel.
    download::DownloadItem* download =
        download_manager_->GetDownload(download_id);
    DCHECK(download);
    download_ui_model = AddDownload(DownloadItemModel::Wrap(download));
    DCHECK(download_ui_model);
  }

  if (embedder()) {
    embedder()->ShowDownloadContextMenu(download_ui_model,
                                        gfx::Point(client_x, client_y));
  }
}

void DownloadShelfUI::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download_model) {
  DownloadUIModel* download = AddDownload(std::move(download_model));

  if (page_handler_)
    page_handler_->DoShowDownload(download);
}

DownloadUIModel* DownloadShelfUI::AddDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  DownloadUIModel* pointer = download.get();
  items_.insert_or_assign(download->download()->GetId(), std::move(download));
  return pointer;
}

DownloadUIModel* DownloadShelfUI::FindDownloadById(uint32_t download_id) const {
  return items_.count(download_id) ? items_.at(download_id).get() : nullptr;
}
