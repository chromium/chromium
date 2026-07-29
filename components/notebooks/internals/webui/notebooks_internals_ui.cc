// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internals/webui/notebooks_internals_ui.h"

#include <utility>

#include "components/grit/notebooks_internals_resources.h"
#include "components/grit/notebooks_internals_resources_map.h"
#include "components/notebooks/internals/webui/notebooks_internals_page_handler.h"
#include "components/notebooks/public/notebooks_constants.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace notebooks {

NotebooksInternalsUI::NotebooksInternalsUI(
    content::WebUI* web_ui,
    NotebooksEligibilityService* notebooks_eligibility_service)
    : ui::MojoWebUIController(web_ui),
      notebooks_eligibility_service_(notebooks_eligibility_service) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUINotebooksInternalsHost);

  webui::SetupWebUIDataSource(source, kNotebooksInternalsResources,
                              IDR_NOTEBOOKS_INTERNALS_NOTEBOOKS_INTERNALS_HTML);
}

NotebooksInternalsUI::~NotebooksInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(NotebooksInternalsUI)

void NotebooksInternalsUI::BindInterface(
    mojo::PendingReceiver<notebooks_internals::mojom::PageHandlerFactory>
        receiver) {
  page_handler_factory_receiver_.reset();
  page_handler_factory_receiver_.Bind(std::move(receiver));
}

void NotebooksInternalsUI::CreatePageHandler(
    mojo::PendingRemote<notebooks_internals::mojom::Page> page,
    mojo::PendingReceiver<notebooks_internals::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<NotebooksInternalsPageHandler>(
      std::move(receiver), std::move(page),
      web_ui()->GetWebContents()->GetBrowserContext(),
      notebooks_eligibility_service_);
}

}  // namespace notebooks
