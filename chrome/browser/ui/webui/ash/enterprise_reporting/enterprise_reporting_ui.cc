// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_ui.h"

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/enterprise_reporting_resources.h"
#include "chrome/grit/enterprise_reporting_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::reporting {

EnterpriseReportingUI::EnterpriseReportingUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
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

}  // namespace ash::reporting
