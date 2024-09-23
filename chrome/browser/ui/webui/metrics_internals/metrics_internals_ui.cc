// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_internals/field_trials_handler.h"
#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "components/grit/metrics_internals_resources.h"
#include "components/grit/metrics_internals_resources_map.h"
#include "components/metrics/structured/buildflags/buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)
#include "chrome/browser/ui/webui/metrics_internals/structured_metrics_internals_handler.h"
#endif

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

  web_ui->AddMessageHandler(
      std::make_unique<FieldTrialsHandler>(Profile::FromBrowserContext(
          web_ui->GetWebContents()->GetBrowserContext())));

// Set up the resource and message handler for
// chrome://metrics-internals/structured.
#if BUILDFLAG(STRUCTURED_METRICS_DEBUG_ENABLED)
  source->AddResourcePath(
      "structured", IDR_METRICS_INTERNALS_STRUCTURED_STRUCTURED_INTERNALS_HTML);
  source->AddResourcePath(
      "structured/",
      IDR_METRICS_INTERNALS_STRUCTURED_STRUCTURED_INTERNALS_HTML);
  web_ui->AddMessageHandler(
      std::make_unique<StructuredMetricsInternalsHandler>());
#endif
}
