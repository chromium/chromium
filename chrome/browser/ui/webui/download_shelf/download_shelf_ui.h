// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf.mojom.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_page_handler.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui_embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class DownloadShelfUI : public ui::MojoWebUIController,
                        public download_shelf::mojom::PageHandlerFactory {
 public:
  explicit DownloadShelfUI(content::WebUI* web_ui);
  DownloadShelfUI(const DownloadShelfUI&) = delete;
  DownloadShelfUI& operator=(const DownloadShelfUI&) = delete;
  ~DownloadShelfUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<download_shelf::mojom::PageHandlerFactory>
          receiver);

  void set_embedder(DownloadShelfUIEmbedder* embedder) { embedder_ = embedder; }
  DownloadShelfUIEmbedder* embedder() const { return embedder_; }

  void ShowContextMenu(uint32_t download_id,
                       int32_t client_x,
                       int32_t client_y);

  void DoShowDownload(DownloadUIModel::DownloadUIModelPtr download_model);

 private:
  // download_shelf::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<download_shelf::mojom::Page> page,
      mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver)
      override;

  DownloadUIModel* AddDownload(DownloadUIModel::DownloadUIModelPtr download);

  DownloadUIModel* FindDownloadById(uint32_t download_id) const;

  std::unique_ptr<DownloadShelfPageHandler> page_handler_;

  mojo::Receiver<download_shelf::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  content::DownloadManager* const download_manager_;
  DownloadShelfUIEmbedder* embedder_ = nullptr;

  base::flat_map<uint32_t, DownloadUIModel::DownloadUIModelPtr> items_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_H_
