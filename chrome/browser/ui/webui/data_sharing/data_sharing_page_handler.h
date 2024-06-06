// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/data_sharing/data_sharing.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DataSharingPageHandler : public data_sharing::mojom::PageHandler {
 public:
  DataSharingPageHandler(
      TopChromeWebUIController* webui_controller,
      mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver);

  DataSharingPageHandler(const DataSharingPageHandler&) = delete;
  DataSharingPageHandler& operator=(const DataSharingPageHandler&) = delete;

  ~DataSharingPageHandler() override;

  void ShowUI() override;

 private:
  // webui_controller_ owns DataSharingPageHandler and outlives it.
  const raw_ptr<TopChromeWebUIController> webui_controller_;

  mojo::Receiver<data_sharing::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_PAGE_HANDLER_H_
