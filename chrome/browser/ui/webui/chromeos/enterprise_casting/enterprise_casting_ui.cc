// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/enterprise_casting/enterprise_casting_ui.h"

#include "base/containers/span.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/enterprise_casting_resources.h"
#include "chrome/grit/enterprise_casting_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

EnterpriseCastingUI::EnterpriseCastingUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(chrome::kChromeUIEnterpriseCastingHost));
  webui::SetupWebUIDataSource(source.get(),
                              base::make_span(kEnterpriseCastingResources,
                                              kEnterpriseCastingResourcesSize),
                              IDR_ENTERPRISE_CASTING_INDEX_HTML);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

EnterpriseCastingUI::~EnterpriseCastingUI() = default;

void EnterpriseCastingUI::BindInterface(
    mojo::PendingReceiver<enterprise_casting::mojom::PageHandlerFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void EnterpriseCastingUI::CreatePageHandler(
    mojo::PendingRemote<enterprise_casting::mojom::Page> page,
    mojo::PendingReceiver<enterprise_casting::mojom::PageHandler> receiver) {
  DCHECK(page);

  page_handler_ = std::make_unique<EnterpriseCastingHandler>(
      std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(EnterpriseCastingUI)

}  // namespace chromeos
