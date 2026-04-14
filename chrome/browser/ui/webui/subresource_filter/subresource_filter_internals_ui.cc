// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/subresource_filter/subresource_filter_internals_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/subresource_filter/subresource_filter_internals_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/subresource_filter_internals_resources.h"
#include "chrome/grit/subresource_filter_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_util.h"

namespace subresource_filter {

SubresourceFilterInternalsUI::SubresourceFilterInternalsUI(
    content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUISubresourceFilterInternalsHost);

  webui::SetupWebUIDataSource(source, kSubresourceFilterInternalsResources,
                              IDR_SUBRESOURCE_FILTER_INTERNALS_INDEX_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  handler_ = std::make_unique<SubresourceFilterInternalsHandler>(profile);
}

SubresourceFilterInternalsUI::~SubresourceFilterInternalsUI() = default;

void SubresourceFilterInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::SubresourceFilterInternalsHandler> receiver) {
  handler_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(SubresourceFilterInternalsUI)

}  // namespace subresource_filter
