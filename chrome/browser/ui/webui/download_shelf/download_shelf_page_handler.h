// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/download_shelf/download_shelf.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

class DownloadShelfPageHandler : public download_shelf::mojom::PageHandler {
 public:
  DownloadShelfPageHandler(
      mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver,
      mojo::PendingRemote<download_shelf::mojom::Page> page,
      content::WebUI* web_ui,
      ui::MojoWebUIController* webui_controller);
  DownloadShelfPageHandler(const DownloadShelfPageHandler&) = delete;
  DownloadShelfPageHandler& operator=(const DownloadShelfPageHandler&) = delete;
  ~DownloadShelfPageHandler() override;

 private:
  mojo::Receiver<download_shelf::mojom::PageHandler> receiver_;
  mojo::Remote<download_shelf::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_PAGE_HANDLER_H_
