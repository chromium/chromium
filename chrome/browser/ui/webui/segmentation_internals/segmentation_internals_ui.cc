// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_page_handler_impl.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/segmentation_internals_resources.h"
#include "chrome/grit/segmentation_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

SegmentationInternalsUI::SegmentationInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  profile_ = Profile::FromWebUI(web_ui);
  auto source = base::WrapUnique(content::WebUIDataSource::Create(
      chrome::kChromeUISegmentationInternalsHost));
  webui::SetupWebUIDataSource(
      source.get(),
      base::make_span(kSegmentationInternalsResources,
                      kSegmentationInternalsResourcesSize),
      IDR_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

SegmentationInternalsUI::~SegmentationInternalsUI() = default;

void SegmentationInternalsUI::BindInterface(
    mojo::PendingReceiver<segmentation_internals::mojom::PageHandlerFactory>
        receiver) {
  segmentation_internals_page_factory_receiver_.reset();
  segmentation_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void SegmentationInternalsUI::CreatePageHandler(
    mojo::PendingRemote<segmentation_internals::mojom::Page> page,
    mojo::PendingReceiver<segmentation_internals::mojom::PageHandler>
        receiver) {
  segmentation_internals_page_handler_ =
      std::make_unique<SegmentationInternalsPageHandlerImpl>(
          std::move(receiver), std::move(page), profile_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(SegmentationInternalsUI)
