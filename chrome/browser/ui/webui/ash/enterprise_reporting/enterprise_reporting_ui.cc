// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_ui.h"

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/enterprise_reporting_resources.h"
#include "chrome/grit/enterprise_reporting_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::reporting {

EnterpriseReportingUI::EnterpriseReportingUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  DCHECK(base::FeatureList::IsEnabled(ash::features::kEnterpriseReportingUI));
  // Set up the chrome://enterprise-reporting source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIEnterpriseReportingHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kEnterpriseReportingResources,
                      kEnterpriseReportingResourcesSize),
      IDR_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_HTML);
}

EnterpriseReportingUI::~EnterpriseReportingUI() = default;

bool EnterpriseReportingUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(ash::features::kEnterpriseReportingUI);
}

WEB_UI_CONTROLLER_TYPE_IMPL(EnterpriseReportingUI)

void EnterpriseReportingUI::BindInterface(
    mojo::PendingReceiver<enterprise_reporting::mojom::PageHandlerFactory>
        receiver) {
  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void EnterpriseReportingUI::CreatePageHandler(
    mojo::PendingRemote<enterprise_reporting::mojom::Page> page,
    mojo::PendingReceiver<enterprise_reporting::mojom::PageHandler> receiver) {
  DCHECK(page);
  // Ensure sequencing for `EnterpriseReportingPageHandler` in order to make it
  // capable of using weak pointers.
  page_handler_ = EnterpriseReportingPageHandler::Create(std::move(receiver),
                                                         std::move(page));
}
}  // namespace ash::reporting
