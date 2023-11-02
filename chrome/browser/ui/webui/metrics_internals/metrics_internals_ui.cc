// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_ui.h"

#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "components/grit/metrics_internals_resources.h"
#include "components/grit/metrics_internals_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

MetricsInternalsUI::MetricsInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://metrics-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIMetricsInternalsHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source,
                              base::make_span(kMetricsInternalsResources,
                                              kMetricsInternalsResourcesSize),
                              IDR_METRICS_INTERNALS_METRICS_INTERNALS_HTML);

  web_ui->AddMessageHandler(std::make_unique<MetricsInternalsHandler>());
}
