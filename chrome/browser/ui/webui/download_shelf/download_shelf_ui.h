// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf.mojom.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_handler.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui_embedder.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

using download::DownloadItem;

class DownloadShelfUI : public ui::MojoWebUIController,
                        public download_shelf::mojom::PageHandlerFactory,
                        public DownloadItem::Observer {
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

  void DoClose();

  void DoShowAll();

  void DiscardDownload(uint32_t download_id);

  void KeepDownload(uint32_t download_id);

  void ShowContextMenu(uint32_t download_id,
                       int32_t client_x,
                       int32_t client_y,
                       base::OnceClosure on_menu_will_show_callback);

  void DoShowDownload(DownloadUIModel::DownloadUIModelPtr download_model,
                      base::Time show_download_start_time);

  void OpenDownload(uint32_t download_id);

  // Get the downloads that should be shown on the shelf.
  std::vector<DownloadUIModel*> GetDownloads();

  base::Time GetShowDownloadTime(uint32_t download_id);

  void RemoveDownload(uint32_t download_id);

 protected:
  void SetPageHandlerForTesting(
      std::unique_ptr<DownloadShelfHandler> page_handler);
  void SetProgressTimerForTesting(
      std::unique_ptr<base::RetainingOneShotTimer> timer);

 private:
  // download_shelf::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<download_shelf::mojom::Page> page,
      mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver)
      override;

  // DownloadItem::Observer
  // The observer calls notify JS side when an download item is updated
  // triggered download shelf or other places e.g. extension API or
  // chrome://downloads.
  void OnDownloadOpened(DownloadItem* download) override;
  void OnDownloadUpdated(DownloadItem* download) override;
  void OnDownloadRemoved(DownloadItem* download) override;
  void OnDownloadDestroyed(DownloadItem* download) override;

  DownloadUIModel* AddDownload(DownloadUIModel::DownloadUIModelPtr download);
  DownloadUIModel* FindDownloadById(uint32_t download_id) const;
  void NotifyDownloadProgress();

  std::unique_ptr<DownloadShelfHandler> page_handler_;
  std::unique_ptr<base::RetainingOneShotTimer> progress_timer_;

  mojo::Receiver<download_shelf::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  const raw_ptr<content::DownloadManager> download_manager_;
  raw_ptr<DownloadShelfUIEmbedder> embedder_ = nullptr;
  WebuiLoadTimer webui_load_timer_;

  // Used to facilitate measuring the time it took from a call to the download
  // shelf's DoShowDownload method to when the associated download item was
  // visible to the user.
  base::flat_map<uint32_t, base::Time> show_download_time_map_;

  base::flat_map<uint32_t, DownloadUIModel::DownloadUIModelPtr> items_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_H_
