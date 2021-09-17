// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf.mojom.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_handler.h"
#include "content/public/browser/download_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

class DownloadShelfUI;

class DownloadShelfPageHandler : public download_shelf::mojom::PageHandler,
                                 public DownloadShelfHandler {
 public:
  DownloadShelfPageHandler(
      mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver,
      mojo::PendingRemote<download_shelf::mojom::Page> page,
      DownloadShelfUI* download_shelf_ui);
  DownloadShelfPageHandler(const DownloadShelfPageHandler&) = delete;
  DownloadShelfPageHandler& operator=(const DownloadShelfPageHandler&) = delete;
  ~DownloadShelfPageHandler() override;

  // download_shelf::mojom::PageHandler:
  void DoShowAll() override;
  void DoClose() override;
  void DiscardDownload(uint32_t download_id) override;
  void KeepDownload(uint32_t download_id) override;
  void GetDownloads(GetDownloadsCallback callback) override;
  void ShowContextMenu(uint32_t download_id,
                       int32_t client_x,
                       int32_t client_y,
                       double timestamp) override;
  void OpenDownload(uint32_t download_id) override;

  // DownloadShelfHandler:
  void DoShowDownload(DownloadUIModel* download_model) override;
  void OnDownloadOpened(uint32_t download_id) override;
  void OnDownloadUpdated(DownloadUIModel* download_model) override;
  void OnDownloadErased(uint32_t download_id) override;

 private:
  download_shelf::mojom::DownloadItemPtr GetDownloadItemFromUIModel(
      DownloadUIModel* download_model);

  mojo::Receiver<download_shelf::mojom::PageHandler> receiver_;
  mojo::Remote<download_shelf::mojom::Page> page_;

  const raw_ptr<DownloadShelfUI> download_shelf_ui_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_PAGE_HANDLER_H_
