// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_ui.h"

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_page_handler_impl.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/data_sharing_internals_resources.h"
#include "chrome/grit/data_sharing_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

DataSharingInternalsUIConfig::DataSharingInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIDataSharingInternalsHost) {}

DataSharingInternalsUIConfig::~DataSharingInternalsUIConfig() = default;

DataSharingInternalsUI::DataSharingInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDataSharingInternalsHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kDataSharingInternalsResources,
                      kDataSharingInternalsResourcesSize),
      IDR_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_HTML);
}

DataSharingInternalsUI::~DataSharingInternalsUI() = default;

void DataSharingInternalsUI::BindInterface(
    mojo::PendingReceiver<data_sharing_internals::mojom::PageHandlerFactory>
        receiver) {
  data_sharing_internals_page_factory_receiver_.reset();
  data_sharing_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void DataSharingInternalsUI::CreatePageHandler(
    mojo::PendingRemote<data_sharing_internals::mojom::Page> page,
    mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler>
        receiver) {
  data_sharing_internals_page_handler_ =
      std::make_unique<DataSharingInternalsPageHandlerImpl>(
          std::move(receiver), std::move(page),
          data_sharing::DataSharingServiceFactory::GetForProfile(
              Profile::FromWebUI(web_ui())));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DataSharingInternalsUI)
