// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals.mojom.h"
#include "components/data_sharing/public/protocol/group_data.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

class DataSharingInternalsPageHandlerImpl;
class DataSharingInternalsUI;

class DataSharingInternalsUIConfig
    : public content::DefaultWebUIConfig<DataSharingInternalsUI> {
 public:
  DataSharingInternalsUIConfig();
  ~DataSharingInternalsUIConfig() override;
};

// The WebUI controller for chrome://data-sharing-internals.
class DataSharingInternalsUI
    : public ui::MojoWebUIController,
      public data_sharing_internals::mojom::PageHandlerFactory {
 public:
  explicit DataSharingInternalsUI(content::WebUI* web_ui);
  ~DataSharingInternalsUI() override;

  DataSharingInternalsUI(const DataSharingInternalsUI&) = delete;
  DataSharingInternalsUI& operator=(const DataSharingInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<data_sharing_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // data_sharing_internals::mojom::PageHandlerFactory impls.
  void CreatePageHandler(
      mojo::PendingRemote<data_sharing_internals::mojom::Page> page,
      mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler>
          receiver) override;

  std::unique_ptr<DataSharingInternalsPageHandlerImpl>
      data_sharing_internals_page_handler_;
  mojo::Receiver<data_sharing_internals::mojom::PageHandlerFactory>
      data_sharing_internals_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_
